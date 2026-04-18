#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <kernel/fat32.h>
#include <kernel/disk.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>

// Static filesystem context
static fat32_fs_t fs;
static uint32_t current_device = 0;

// Sector buffer (512 bytes)
static uint8_t sector_buffer[512];
static uint8_t fat_buffer[512];

// Helper: Check if filesystem is FAT32
static bool is_fat32(fat32_bootsector_t* boot) {
    // FAT32 has root_entries = 0 and total_sectors_16 = 0
    if (boot->root_entries != 0 || boot->total_sectors_16 != 0) {
        return false;
    }
    // Check for "FAT32" string
    if (strncmp((char*)boot->fs_type, "FAT32", 5) != 0) {
        return false;
    }
    return true;
}

// Read sector from disk
bool fat32_read_sector(uint32_t sector, void* buffer) {
    return disk_read_sectors(current_device, sector, 1, buffer);
}

// Write sector to disk
bool fat32_write_sector(uint32_t sector, const void* buffer) {
    return disk_write_sectors(current_device, sector, 1, buffer);
}

// Initialize FAT32 filesystem
bool fat32_init(void) {
    memset(&fs, 0, sizeof(fs));
    return true;
}

// Mount FAT32 filesystem
bool fat32_mount(uint32_t device_id) {
    current_device = device_id;
    
    // Read boot sector
    if (!fat32_read_sector(0, &fs.bootsector)) {
        printf("FAT32: Failed to read boot sector\n");
        return false;
    }
    
    // Verify FAT32 signature
    if (fs.bootsector.boot_jump[0] != 0xEB && fs.bootsector.boot_jump[0] != 0xE9) {
        printf("FAT32: Invalid boot sector signature\n");
        return false;
    }
    
    // Check if it's actually FAT32
    if (!is_fat32(&fs.bootsector)) {
        printf("FAT32: Not a FAT32 filesystem\n");
        return false;
    }
    
    // Calculate important values
    fs.sectors_per_cluster = fs.bootsector.sectors_per_cluster;
    fs.bytes_per_cluster = fs.bootsector.bytes_per_sector * fs.sectors_per_cluster;
    
    // FAT start sector
    fs.fat_start_sector = fs.bootsector.reserved_sectors;
    
    // Calculate number of sectors per FAT
    uint32_t fat_size = fs.bootsector.fat_size_32;
    
    // Data start sector
    fs.data_start_sector = fs.fat_start_sector + (fs.bootsector.num_fats * fat_size);
    
    // Total clusters
    uint32_t total_sectors = fs.bootsector.total_sectors_32;
    uint32_t data_sectors = total_sectors - fs.data_start_sector;
    fs.total_clusters = data_sectors / fs.sectors_per_cluster;
    
    // Root directory cluster
    // In FAT32, root is at cluster specified in boot sector
    fs.root_dir_sector = fat32_cluster_to_sector(fs.bootsector.root_cluster);
    
    printf("FAT32: Mounted successfully\n");
    printf("  Volume: %.11s\n", fs.bootsector.volume_label);
    printf("  Sectors per cluster: %u\n", fs.sectors_per_cluster);
    printf("  Total clusters: %u\n", fs.total_clusters);
    printf("  Root cluster: %u\n", fs.bootsector.root_cluster);
    
    fs.mounted = true;
    return true;
}

// Unmount filesystem
void fat32_unmount(void) {
    if (fs.fat_cache) {
        // Free cache
        fs.fat_cache = NULL;
    }
    fs.mounted = false;
}

// Check if filesystem is mounted
bool fat32_is_mounted(void) {
    return fs.mounted;
}

// Convert cluster number to sector number
uint32_t fat32_cluster_to_sector(uint32_t cluster) {
    if (cluster < 2) return 0;
    return fs.data_start_sector + ((cluster - 2) * fs.sectors_per_cluster);
}

// Get next cluster in chain from FAT
uint32_t fat32_get_next_cluster(uint32_t cluster) {
    if (cluster < 2 || cluster >= fs.total_clusters + 2) {
        return FAT_CLUSTER_END;
    }
    
    // Calculate FAT entry position
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    // Read FAT sector
    if (!fat32_read_sector(fat_sector, fat_buffer)) {
        return FAT_CLUSTER_END;
    }
    
    // Read 32-bit entry
    uint32_t next_cluster = *(uint32_t*)(fat_buffer + entry_offset);
    next_cluster &= 0x0FFFFFFF;  // Mask to 28 bits
    
    return next_cluster;
}

// Read cluster data
static bool read_cluster(uint32_t cluster, void* buffer) {
    uint32_t sector = fat32_cluster_to_sector(cluster);
    for (uint32_t i = 0; i < fs.sectors_per_cluster; i++) {
        if (!fat32_read_sector(sector + i, (uint8_t*)buffer + (i * 512))) {
            return false;
        }
    }
    return true;
}

// Convert 8.3 filename to normal string
static void format_short_name(const uint8_t* src, char* dest) {
    int i, j = 0;
    
    // Copy name (first 8 chars, strip spaces)
    for (i = 0; i < 8 && src[i] != ' '; i++) {
        dest[j++] = tolower(src[i]);
    }
    
    // Add extension if present
    if (src[8] != ' ') {
        dest[j++] = '.';
        for (i = 8; i < 11 && src[i] != ' '; i++) {
            dest[j++] = tolower(src[i]);
        }
    }
    
    dest[j] = '\0';
}

// Find directory entry
static bool find_entry(uint32_t dir_cluster, const char* name, 
                       fat32_direntry_t* entry_out, uint32_t* entry_cluster) {
    uint8_t cluster_buffer[4096];  // Max cluster size
    char formatted_name[256];
    
    while (dir_cluster < FAT_CLUSTER_END) {
        // Read cluster
        if (!read_cluster(dir_cluster, cluster_buffer)) {
            return false;
        }
        
        // Parse directory entries
        fat32_direntry_t* entries = (fat32_direntry_t*)cluster_buffer;
        uint32_t entries_per_cluster = fs.bytes_per_cluster / sizeof(fat32_direntry_t);
        
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            // End of directory
            if (entries[i].name[0] == 0x00) {
                return false;
            }
            
            // Deleted entry
            if (entries[i].name[0] == 0xE5) {
                continue;
            }
            
            // Skip LFN entries
            if (entries[i].attributes == FAT_ATTR_LFN) {
                continue;
            }
            
            // Skip volume label
            if (entries[i].attributes & FAT_ATTR_VOLUME_ID) {
                continue;
            }
            
            // Format entry name
            format_short_name(entries[i].name, formatted_name);
            
            // Compare
            if (strcmp(formatted_name, name) == 0) {
                *entry_out = entries[i];
                if (entry_cluster) *entry_cluster = dir_cluster;
                return true;
            }
        }
        
        // Get next cluster
        dir_cluster = fat32_get_next_cluster(dir_cluster);
    }
    
    return false;
}

// Open a file
bool fat32_open(fat32_file_t* file, const char* path) {
    if (!fs.mounted || !file || !path) return false;
    
    // Start from root directory
    uint32_t dir_cluster = fs.bootsector.root_cluster;
    
    // Parse path
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';
    
    char* token = strtok(path_copy, "/");
    fat32_direntry_t entry;
    
    while (token) {
        // Find entry in current directory
        if (!find_entry(dir_cluster, token, &entry, NULL)) {
            return false;
        }
        
        // Get cluster from entry
        uint32_t cluster = ((uint32_t)entry.cluster_high << 16) | entry.cluster_low;
        
        token = strtok(NULL, "/");
        if (token) {
            // More path components - must be directory
            if (!(entry.attributes & FAT_ATTR_DIRECTORY)) {
                return false;
            }
            dir_cluster = cluster;
        } else {
            // Final component - this is our file
            strncpy(file->name, token ? token : path, 255);
            file->cluster = cluster;
            file->size = entry.file_size;
            file->attributes = entry.attributes;
            file->position = 0;
            file->open = true;
            file->is_directory = (entry.attributes & FAT_ATTR_DIRECTORY) != 0;
        }
    }
    
    return true;
}

// Close a file
void fat32_close(fat32_file_t* file) {
    if (file) {
        file->open = false;
    }
}

// Read from file
size_t fat32_read(fat32_file_t* file, void* buffer, size_t size) {
    if (!file || !file->open || !buffer) return 0;
    if (file->is_directory) return 0;
    
    // Don't read past end of file
    if (file->position + size > file->size) {
        size = file->size - file->position;
    }
    if (size == 0) return 0;
    
    uint8_t* dest = (uint8_t*)buffer;
    size_t bytes_read = 0;
    
    // Calculate starting cluster and offset
    uint32_t cluster = file->cluster;
    uint32_t cluster_offset = file->position / fs.bytes_per_cluster;
    uint32_t byte_offset = file->position % fs.bytes_per_cluster;
    
    // Seek to correct cluster
    for (uint32_t i = 0; i < cluster_offset && cluster < FAT_CLUSTER_END; i++) {
        cluster = fat32_get_next_cluster(cluster);
    }
    
    // Read data
    while (bytes_read < size && cluster < FAT_CLUSTER_END) {
        // Read cluster
        uint8_t cluster_buffer[4096];
        if (!read_cluster(cluster, cluster_buffer)) {
            break;
        }
        
        // Copy data
        uint32_t to_copy = fs.bytes_per_cluster - byte_offset;
        if (to_copy > size - bytes_read) {
            to_copy = size - bytes_read;
        }
        
        memcpy(dest + bytes_read, cluster_buffer + byte_offset, to_copy);
        bytes_read += to_copy;
        file->position += to_copy;
        byte_offset = 0;
        
        // Get next cluster
        cluster = fat32_get_next_cluster(cluster);
    }
    
    return bytes_read;
}

// Seek in file
bool fat32_seek(fat32_file_t* file, uint32_t position) {
    if (!file || !file->open) return false;
    if (position > file->size) return false;
    
    file->position = position;
    return true;
}

// Open directory
bool fat32_opendir(fat32_dir_t* dir, const char* path) {
    if (!fs.mounted || !dir) return false;
    
    uint32_t cluster;
    
    if (path == NULL || strcmp(path, "/") == 0 || strlen(path) == 0) {
        // Root directory
        cluster = fs.bootsector.root_cluster;
    } else {
        // Find directory
        fat32_file_t file;
        if (!fat32_open(&file, path)) return false;
        if (!file.is_directory) {
            fat32_close(&file);
            return false;
        }
        cluster = file.cluster;
    }
    
    dir->cluster = cluster;
    dir->position = 0;
    dir->open = true;
    
    return true;
}

// Close directory
void fat32_closedir(fat32_dir_t* dir) {
    if (dir) {
        dir->open = false;
    }
}

// Read directory entry
bool fat32_readdir(fat32_dir_t* dir, fat32_direntry_t* entry, char* name) {
    if (!dir || !dir->open || !entry) return false;
    
    uint8_t cluster_buffer[4096];
    
    while (dir->cluster < FAT_CLUSTER_END) {
        // Read cluster
        if (!read_cluster(dir->cluster, cluster_buffer)) {
            return false;
        }
        
        // Calculate entry position
        uint32_t entries_per_cluster = fs.bytes_per_cluster / sizeof(fat32_direntry_t);
        uint32_t entry_idx = dir->position % entries_per_cluster;
        
        // Check if we need to move to next cluster
        if (entry_idx == 0 && dir->position > 0) {
            uint32_t next = fat32_get_next_cluster(dir->cluster);
            if (next >= FAT_CLUSTER_END) {
                return false;
            }
            dir->cluster = next;
            continue;
        }
        
        fat32_direntry_t* entries = (fat32_direntry_t*)cluster_buffer;
        
        // End of directory
        if (entries[entry_idx].name[0] == 0x00) {
            return false;
        }
        
        // Skip deleted entries
        if (entries[entry_idx].name[0] == 0xE5) {
            dir->position++;
            continue;
        }
        
        // Skip LFN entries
        if (entries[entry_idx].attributes == FAT_ATTR_LFN) {
            dir->position++;
            continue;
        }
        
        // Skip volume label
        if (entries[entry_idx].attributes & FAT_ATTR_VOLUME_ID) {
            dir->position++;
            continue;
        }
        
        // Found valid entry
        *entry = entries[entry_idx];
        if (name) {
            format_short_name(entry->name, name);
        }
        
        dir->position++;
        return true;
    }
    
    return false;
}

// Find a free cluster
uint32_t fat32_allocate_cluster(void) {
    // Start from cluster 2 (first valid data cluster)
    for (uint32_t cluster = 2; cluster < fs.total_clusters + 2; cluster++) {
        uint32_t next = fat32_get_next_cluster(cluster);
        if (next == FAT_CLUSTER_FREE) {
            // Mark as end of chain
            fat32_set_cluster(cluster, FAT_CLUSTER_END);
            return cluster;
        }
    }
    return 0;  // No free clusters
}

// Set cluster value in FAT
void fat32_set_cluster(uint32_t cluster, uint32_t value) {
    if (cluster < 2 || cluster >= fs.total_clusters + 2) return;
    
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    // Read FAT sector
    if (!fat32_read_sector(fat_sector, fat_buffer)) return;
    
    // Set value (28-bit)
    uint32_t* entry = (uint32_t*)(fat_buffer + entry_offset);
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);
    
    // Write back
    fat32_write_sector(fat_sector, fat_buffer);
    
    // Update backup FAT if present
    if (fs.bootsector.num_fats > 1) {
        uint32_t fat_size = fs.bootsector.fat_size_32;
        fat32_write_sector(fat_sector + fat_size, fat_buffer);
    }
}

// Free a cluster chain
void fat32_free_cluster_chain(uint32_t start_cluster) {
    uint32_t cluster = start_cluster;
    while (cluster >= FAT_CLUSTER_MIN && cluster <= FAT_CLUSTER_MAX) {
        uint32_t next = fat32_get_next_cluster(cluster);
        fat32_set_cluster(cluster, FAT_CLUSTER_FREE);
        cluster = next;
    }
}

// Write cluster data
static bool write_cluster(uint32_t cluster, const void* buffer) {
    uint32_t sector = fat32_cluster_to_sector(cluster);
    for (uint32_t i = 0; i < fs.sectors_per_cluster; i++) {
        if (!fat32_write_sector(sector + i, (uint8_t*)buffer + (i * 512))) {
            return false;
        }
    }
    return true;
}

// Extend file by one cluster
static uint32_t extend_file(uint32_t last_cluster) {
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) return 0;
    
    // Clear the new cluster
    uint8_t zero_buffer[4096] = {0};
    write_cluster(new_cluster, zero_buffer);
    
    // Link to previous cluster
    if (last_cluster >= FAT_CLUSTER_MIN) {
        fat32_set_cluster(last_cluster, new_cluster);
    }
    
    return new_cluster;
}

// Write to file
size_t fat32_write(fat32_file_t* file, const void* buffer, size_t size) {
    if (!file || !file->open || !buffer) return 0;
    if (file->is_directory) return 0;
    if (size == 0) return 0;
    
    const uint8_t* src = (const uint8_t*)buffer;
    size_t bytes_written = 0;
    
    // If file has no clusters, allocate first one
    if (file->cluster < 2) {
        uint32_t new_cluster = fat32_allocate_cluster();
        if (new_cluster == 0) return 0;
        file->cluster = new_cluster;
    }
    
    // Calculate starting position
    uint32_t cluster = file->cluster;
    uint32_t cluster_offset = file->position / fs.bytes_per_cluster;
    uint32_t byte_offset = file->position % fs.bytes_per_cluster;
    
    // Seek to correct cluster
    uint32_t last_cluster = 0;
    for (uint32_t i = 0; i < cluster_offset; i++) {
        uint32_t next = fat32_get_next_cluster(cluster);
        if (next >= FAT_CLUSTER_END) {
            // Need to extend file
            next = extend_file(cluster);
            if (next == 0) break;
        }
        last_cluster = cluster;
        cluster = next;
    }
    
    // Write data
    uint8_t cluster_buffer[4096];
    while (bytes_written < size) {
        // Read existing cluster data (for partial writes)
        if (byte_offset > 0 || (size - bytes_written) < fs.bytes_per_cluster) {
            if (!read_cluster(cluster, cluster_buffer)) break;
        }
        
        // Copy data
        uint32_t to_copy = fs.bytes_per_cluster - byte_offset;
        if (to_copy > size - bytes_written) {
            to_copy = size - bytes_written;
        }
        
        memcpy(cluster_buffer + byte_offset, src + bytes_written, to_copy);
        bytes_written += to_copy;
        file->position += to_copy;
        byte_offset = 0;
        
        // Write cluster back
        if (!write_cluster(cluster, cluster_buffer)) break;
        
        // Get or allocate next cluster
        if (bytes_written < size) {
            uint32_t next = fat32_get_next_cluster(cluster);
            if (next >= FAT_CLUSTER_END) {
                next = extend_file(cluster);
                if (next == 0) break;
            }
            cluster = next;
        }
    }
    
    // Update file size if we wrote past the end
    if (file->position > file->size) {
        file->size = file->position;
    }
    
    return bytes_written;
}

// Find free directory entry
static bool find_free_entry(uint32_t dir_cluster, fat32_direntry_t** entry_out, uint8_t* cluster_buffer) {
    while (dir_cluster < FAT_CLUSTER_END) {
        if (!read_cluster(dir_cluster, cluster_buffer)) return false;
        
        fat32_direntry_t* entries = (fat32_direntry_t*)cluster_buffer;
        uint32_t entries_per_cluster = fs.bytes_per_cluster / sizeof(fat32_direntry_t);
        
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            // Free slot found (deleted or end of directory)
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                *entry_out = &entries[i];
                return true;
            }
        }
        
        // Get next cluster
        uint32_t next = fat32_get_next_cluster(dir_cluster);
        if (next >= FAT_CLUSTER_END) {
            // Extend directory
            next = extend_file(dir_cluster);
            if (next == 0) return false;
            
            // Clear new cluster
            memset(cluster_buffer, 0, fs.bytes_per_cluster);
            write_cluster(next, cluster_buffer);
            
            *entry_out = (fat32_direntry_t*)cluster_buffer;
            return true;
        }
        dir_cluster = next;
    }
    return false;
}

// Convert filename to 8.3 format
static bool to_short_name(const char* name, uint8_t* short_name) {
    // Initialize with spaces
    memset(short_name, ' ', 11);
    
    // Copy name part
    int i = 0;
    while (name[i] && name[i] != '.' && i < 8) {
        short_name[i] = toupper(name[i]);
        i++;
    }
    
    // Find extension
    const char* ext = strchr(name, '.');
    if (ext) {
        ext++;  // Skip dot
        int j = 0;
        while (ext[j] && j < 3) {
            short_name[8 + j] = toupper(ext[j]);
            j++;
        }
    }
    
    return true;
}

// Create a new file or directory
bool fat32_create(const char* path, uint8_t attributes) {
    if (!fs.mounted) return false;
    
    // Parse path to get directory and filename
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';
    
    // Find last slash to separate directory and filename
    char* filename = strrchr(path_copy, '/');
    uint32_t dir_cluster;
    
    if (filename) {
        *filename = '\0';
        filename++;
        
        // Get directory cluster
        if (strlen(path_copy) == 0 || strcmp(path_copy, "/") == 0) {
            dir_cluster = fs.bootsector.root_cluster;
        } else {
            fat32_file_t dir_file;
            if (!fat32_open(&dir_file, path_copy)) return false;
            if (!dir_file.is_directory) {
                fat32_close(&dir_file);
                return false;
            }
            dir_cluster = dir_file.cluster;
        }
    } else {
        filename = path_copy;
        dir_cluster = fs.bootsector.root_cluster;
    }
    
    // Check if file already exists
    fat32_direntry_t existing;
    if (find_entry(dir_cluster, filename, &existing, NULL)) {
        return false;  // File exists
    }
    
    // Allocate cluster for file (if not empty file)
    uint32_t new_cluster = 0;
    if (attributes & FAT_ATTR_DIRECTORY) {
        new_cluster = fat32_allocate_cluster();
        if (new_cluster == 0) return false;
        
        // Clear new directory cluster
        uint8_t zero_buffer[4096] = {0};
        write_cluster(new_cluster, zero_buffer);
    }
    
    // Find free directory entry
    uint8_t cluster_buffer[4096];
    fat32_direntry_t* entry = NULL;
    uint32_t entry_cluster = dir_cluster;
    
    while (entry_cluster < FAT_CLUSTER_END) {
        if (!read_cluster(entry_cluster, cluster_buffer)) return false;
        
        fat32_direntry_t* entries = (fat32_direntry_t*)cluster_buffer;
        uint32_t entries_per_cluster = fs.bytes_per_cluster / sizeof(fat32_direntry_t);
        bool found = false;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                entry = &entries[i];
                found = true;
                break;
            }
        }
        
        if (found) break;
        
        // Try next cluster
        uint32_t next = fat32_get_next_cluster(entry_cluster);
        if (next >= FAT_CLUSTER_END) {
            // Extend directory
            next = extend_file(entry_cluster);
            if (next == 0) {
                if (new_cluster) fat32_free_cluster_chain(new_cluster);
                return false;
            }
            memset(cluster_buffer, 0, fs.bytes_per_cluster);
            entry = (fat32_direntry_t*)cluster_buffer;
            break;
        }
        entry_cluster = next;
    }
    
    if (!entry) {
        if (new_cluster) fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    // Initialize entry
    memset(entry, 0, sizeof(fat32_direntry_t));
    to_short_name(filename, entry->name);
    entry->attributes = attributes;
    entry->cluster_high = (new_cluster >> 16) & 0xFFFF;
    entry->cluster_low = new_cluster & 0xFFFF;
    entry->file_size = 0;
    
    // Set creation time/date (simplified)
    entry->creation_date = 0x4A21;  // 2020-01-01
    entry->creation_time = 0x0000;
    entry->modify_date = entry->creation_date;
    entry->modify_time = entry->creation_time;
    entry->access_date = entry->creation_date;
    
    // Write entry back
    if (!write_cluster(entry_cluster, cluster_buffer)) {
        if (new_cluster) fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    return true;
}

// Create a directory
bool fat32_mkdir(const char* path) {
    return fat32_create(path, FAT_ATTR_DIRECTORY);
}

// Delete a file
bool fat32_delete(const char* path) {
    if (!fs.mounted) return false;
    
    // Find the file
    fat32_file_t file;
    if (!fat32_open(&file, path)) return false;
    
    // Free cluster chain
    if (file.cluster >= FAT_CLUSTER_MIN) {
        fat32_free_cluster_chain(file.cluster);
    }
    
    // Mark directory entry as deleted
    // (Simplified - would need to find the actual entry and mark it)
    
    fat32_close(&file);
    return true;
}

// Flush FAT to disk
bool fat32_flush_fat(void) {
    // FAT is already written on each change
    return true;
}