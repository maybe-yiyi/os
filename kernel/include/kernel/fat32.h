#ifndef _KERNEL_FAT32_H
#define _KERNEL_FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// FAT32 Boot Sector (first 512 bytes of partition)
typedef struct __attribute__((packed)) {
    uint8_t  boot_jump[3];          // Boot jump instruction
    uint8_t  oem_name[8];           // OEM Name
    uint16_t bytes_per_sector;      // Bytes per sector (usually 512)
    uint8_t  sectors_per_cluster;   // Sectors per cluster
    uint16_t reserved_sectors;      // Reserved sectors (including boot sector)
    uint8_t  num_fats;              // Number of FAT tables
    uint16_t root_entries;          // Root directory entries (0 for FAT32)
    uint16_t total_sectors_16;      // Total sectors (0 for FAT32)
    uint8_t  media_type;            // Media descriptor
    uint16_t fat_size_16;           // FAT size in sectors (0 for FAT32)
    uint16_t sectors_per_track;     // Sectors per track
    uint16_t num_heads;             // Number of heads
    uint32_t hidden_sectors;        // Hidden sectors before partition
    uint32_t total_sectors_32;      // Total sectors (FAT32)
    uint32_t fat_size_32;           // FAT size in sectors (FAT32)
    uint16_t ext_flags;             // Extended flags
    uint16_t fs_version;            // Filesystem version
    uint32_t root_cluster;          // Root directory cluster
    uint16_t fs_info_sector;        // FSInfo sector
    uint16_t backup_boot_sector;    // Backup boot sector
    uint8_t  reserved[12];          // Reserved
    uint8_t  drive_num;             // Drive number
    uint8_t  reserved1;             // Reserved
    uint8_t  boot_sig;              // Boot signature
    uint32_t volume_id;             // Volume serial number
    uint8_t  volume_label[11];      // Volume label
    uint8_t  fs_type[8];            // Filesystem type string
} fat32_bootsector_t;

// FAT32 Directory Entry
typedef struct __attribute__((packed)) {
    uint8_t  name[11];              // Short filename (8.3 format)
    uint8_t  attributes;            // File attributes
    uint8_t  nt_reserved;           // Reserved for Windows NT
    uint8_t  creation_time_tenths;  // Creation time (tenths of second)
    uint16_t creation_time;         // Creation time
    uint16_t creation_date;         // Creation date
    uint16_t access_date;           // Last access date
    uint16_t cluster_high;          // High 16 bits of cluster number
    uint16_t modify_time;           // Last modification time
    uint16_t modify_date;           // Last modification date
    uint16_t cluster_low;           // Low 16 bits of cluster number
    uint32_t file_size;             // File size in bytes
} fat32_direntry_t;

// Long Filename Entry
typedef struct __attribute__((packed)) {
    uint8_t  order;                 // Entry order (0x40 = last entry)
    uint16_t name1[5];              // First 5 characters (UTF-16)
    uint8_t  attributes;            // Attributes (always 0x0F)
    uint8_t  type;                  // Entry type
    uint8_t  checksum;              // Checksum of short name
    uint16_t name2[6];              // Next 6 characters (UTF-16)
    uint16_t cluster_low;           // Always 0
    uint16_t name3[2];              // Last 2 characters (UTF-16)
} fat32_lfn_entry_t;

// File attributes
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F  // Long filename

// FAT32 special cluster values
#define FAT_CLUSTER_FREE        0x00000000
#define FAT_CLUSTER_RESERVED    0x00000001
#define FAT_CLUSTER_MIN         0x00000002
#define FAT_CLUSTER_MAX         0x0FFFFFEF
#define FAT_CLUSTER_BAD         0x0FFFFFF7
#define FAT_CLUSTER_END         0x0FFFFFFF

// FAT32 filesystem context
typedef struct {
    fat32_bootsector_t bootsector;
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t root_dir_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t total_clusters;
    uint32_t* fat_cache;            // Cached FAT table
    uint32_t fat_cache_size;        // Number of entries in cache
    bool mounted;
} fat32_fs_t;

// File handle
typedef struct {
    char name[256];
    uint32_t cluster;
    uint32_t position;
    uint32_t size;
    uint8_t attributes;
    bool open;
    bool is_directory;
} fat32_file_t;

// Directory handle
typedef struct {
    uint32_t cluster;
    uint32_t position;
    bool open;
} fat32_dir_t;

// Initialize FAT32 filesystem
bool fat32_init(void);

// Mount FAT32 filesystem from device
bool fat32_mount(uint32_t device_id);

// Unmount filesystem
void fat32_unmount(void);

// Open a file
bool fat32_open(fat32_file_t* file, const char* path);

// Close a file
void fat32_close(fat32_file_t* file);

// Read from file
size_t fat32_read(fat32_file_t* file, void* buffer, size_t size);

// Write to file (basic implementation)
size_t fat32_write(fat32_file_t* file, const void* buffer, size_t size);

// Seek in file
bool fat32_seek(fat32_file_t* file, uint32_t position);

// Open directory
bool fat32_opendir(fat32_dir_t* dir, const char* path);

// Close directory
void fat32_closedir(fat32_dir_t* dir);

// Read directory entry
bool fat32_readdir(fat32_dir_t* dir, fat32_direntry_t* entry, char* name);

// Get next cluster in chain
uint32_t fat32_get_next_cluster(uint32_t cluster);

// Convert cluster to sector
uint32_t fat32_cluster_to_sector(uint32_t cluster);

// Read sector from disk
bool fat32_read_sector(uint32_t sector, void* buffer);

// Write sector to disk
bool fat32_write_sector(uint32_t sector, const void* buffer);

// Check if filesystem is mounted
bool fat32_is_mounted(void);

// Get filesystem info
void fat32_get_info(char* volume_label, uint32_t* total_sectors, uint32_t* free_sectors);

// Create a new file
bool fat32_create(const char* path, uint8_t attributes);

// Delete a file
bool fat32_delete(const char* path);

// Create a directory
bool fat32_mkdir(const char* path);

// Allocate a free cluster
uint32_t fat32_allocate_cluster(void);

// Free a cluster chain
void fat32_free_cluster_chain(uint32_t start_cluster);

// Set cluster value in FAT
void fat32_set_cluster(uint32_t cluster, uint32_t value);

// Flush FAT to disk
bool fat32_flush_fat(void);

#endif