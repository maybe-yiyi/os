#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <kernel/disk.h>

// IDE delay - read alternate status register
static inline void ide_delay(uint32_t base) {
    __asm__ __volatile__ ("inb %%dx, %%al" : : "d"(base + IDE_ALT_STATUS) : "al");
    __asm__ __volatile__ ("inb %%dx, %%al" : : "d"(base + IDE_ALT_STATUS) : "al");
    __asm__ __volatile__ ("inb %%dx, %%al" : : "d"(base + IDE_ALT_STATUS) : "al");
    __asm__ __volatile__ ("inb %%dx, %%al" : : "d"(base + IDE_ALT_STATUS) : "al");
}

// Write byte to IDE register
static inline void ide_write(uint32_t base, uint32_t reg, uint8_t data) {
    __asm__ __volatile__ ("outb %%al, %%dx" : : "a"(data), "d"(base + reg));
}

// Read byte from IDE register
static inline uint8_t ide_read(uint32_t base, uint32_t reg) {
    uint8_t data;
    __asm__ __volatile__ ("inb %%dx, %%al" : "=a"(data) : "d"(base + reg));
    return data;
}

// Read word from IDE data register
static inline uint16_t ide_read_word(uint32_t base) {
    uint16_t data;
    __asm__ __volatile__ ("inw %%dx, %%ax" : "=a"(data) : "d"(base + IDE_DATA));
    return data;
}

// Write word to IDE data register
static inline void ide_write_word(uint32_t base, uint16_t data) {
    __asm__ __volatile__ ("outw %%ax, %%dx" : : "a"(data), "d"(base + IDE_DATA));
}

// Disk devices (max 4: primary master/slave, secondary master/slave)
static disk_device_t disks[4];
static uint32_t disk_count = 0;

bool disk_wait_ready(uint32_t base) {
    uint8_t status;
    int timeout = 100000;
    while (timeout--) {
        status = ide_read(base, IDE_STATUS);
        if (!(status & IDE_STAT_BSY) && (status & IDE_STAT_RDY)) {
            return true;
        }
    }
    return false;
}

bool disk_wait_data(uint32_t base) {
    uint8_t status;
    int timeout = 100000;
    while (timeout--) {
        status = ide_read(base, IDE_STATUS);
        if (status & IDE_STAT_ERR) return false;
        if (status & IDE_STAT_DF) return false;
        if (status & IDE_STAT_DRQ) return true;
    }
    return false;
}

void disk_poll(uint32_t base) {
    while (ide_read(base, IDE_STATUS) & IDE_STAT_BSY);
}

static void ide_select_drive(uint32_t base, uint8_t drive) {
    ide_write(base, IDE_DRIVE_SELECT, 0xA0 | (drive << 4));
    ide_delay(base);
}

static bool ide_identify(uint32_t base, uint8_t drive, uint16_t* buffer) {
    ide_select_drive(base, drive);
    
    // Set sector count and LBA registers to 0
    ide_write(base, IDE_SECTOR_COUNT, 0);
    ide_write(base, IDE_LBA_LOW, 0);
    ide_write(base, IDE_LBA_MID, 0);
    ide_write(base, IDE_LBA_HIGH, 0);
    
    // Send IDENTIFY command
    ide_write(base, IDE_COMMAND, IDE_CMD_IDENTIFY);
    ide_delay(base);
    
    // Check if drive exists
    uint8_t status = ide_read(base, IDE_STATUS);
    if (status == 0) return false;  // No drive
    
    // Wait for BSY to clear
    int timeout = 100000;
    while ((ide_read(base, IDE_STATUS) & IDE_STAT_BSY) && timeout--);
    if (timeout == 0) return false;
    
    // Check for ATAPI or SATA
    uint8_t lba_mid = ide_read(base, IDE_LBA_MID);
    uint8_t lba_high = ide_read(base, IDE_LBA_HIGH);
    if (lba_mid != 0 || lba_high != 0) return false;  // ATAPI/SATA drive
    
    // Wait for DRQ
    timeout = 100000;
    while (!(ide_read(base, IDE_STATUS) & IDE_STAT_DRQ) && timeout--);
    if (timeout == 0) return false;
    
    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        buffer[i] = ide_read_word(base);
    }
    
    return true;
}

bool disk_init(void) {
    // Clear disk array
    memset(disks, 0, sizeof(disks));
    disk_count = 0;
    
    // Detect disks
    disk_detect();
    
    return disk_count > 0;
}

void disk_detect(void) {
    uint16_t identify_buffer[256];
    
    // Check primary bus
    uint32_t bases[] = {IDE_PRIMARY_BASE, IDE_SECONDARY_BASE};
    
    for (int bus = 0; bus < 2; bus++) {
        for (int drive = 0; drive < 2; drive++) {
            if (ide_identify(bases[bus], drive, identify_buffer)) {
                disk_device_t* disk = &disks[disk_count];
                disk->id = disk_count;
                disk->type = DISK_TYPE_IDE;
                disk->bus = bus;
                disk->drive = drive;
                disk->present = true;
                
                // Parse identify data
                disk->cylinders = identify_buffer[1];
                disk->heads = identify_buffer[3];
                disk->sectors_per_track = identify_buffer[6];
                
                // Check for LBA48 support
                disk->lba48 = (identify_buffer[83] & 0x0400) != 0;
                
                // Get total sectors (LBA28 or LBA48)
                if (disk->lba48 && (identify_buffer[103] != 0 || identify_buffer[102] != 0)) {
                    // LBA48 - use words 100-103
                    disk->sectors = ((uint32_t)identify_buffer[102]) | 
                                   (((uint32_t)identify_buffer[103]) << 16);
                } else {
                    // LBA28 - use words 60-61
                    disk->sectors = ((uint32_t)identify_buffer[60]) | 
                                   (((uint32_t)identify_buffer[61]) << 16);
                }
                
                disk_count++;
                
                if (disk_count >= 4) return;
            }
        }
    }
}

disk_device_t* disk_get_device(uint32_t id) {
    if (id >= disk_count) return NULL;
    return &disks[id];
}

uint32_t disk_get_count(void) {
    return disk_count;
}

bool disk_read_sectors(uint32_t device_id, uint32_t lba, uint8_t count, void* buffer) {
    if (device_id >= disk_count) return false;
    
    disk_device_t* disk = &disks[device_id];
    if (!disk->present) return false;
    
    uint32_t base = (disk->bus == 0) ? IDE_PRIMARY_BASE : IDE_SECONDARY_BASE;
    
    // Wait for drive to be ready
    if (!disk_wait_ready(base)) return false;
    
    // Select drive
    uint8_t drive_select = 0xE0 | (disk->drive << 4) | ((lba >> 24) & 0x0F);
    ide_write(base, IDE_DRIVE_SELECT, drive_select);
    ide_delay(base);
    
    // Send read command
    ide_write(base, IDE_SECTOR_COUNT, count);
    ide_write(base, IDE_LBA_LOW, lba & 0xFF);
    ide_write(base, IDE_LBA_MID, (lba >> 8) & 0xFF);
    ide_write(base, IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    ide_write(base, IDE_COMMAND, IDE_CMD_READ_SECTORS);
    
    // Read data
    uint16_t* buf = (uint16_t*)buffer;
    for (int sector = 0; sector < count; sector++) {
        // Wait for data ready
        if (!disk_wait_data(base)) return false;
        
        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = ide_read_word(base);
        }
        
        // Delay after read
        ide_delay(base);
    }
    
    return true;
}

bool disk_write_sectors(uint32_t device_id, uint32_t lba, uint8_t count, const void* buffer) {
    if (device_id >= disk_count) return false;
    
    disk_device_t* disk = &disks[device_id];
    if (!disk->present) return false;
    
    uint32_t base = (disk->bus == 0) ? IDE_PRIMARY_BASE : IDE_SECONDARY_BASE;
    
    // Wait for drive to be ready
    if (!disk_wait_ready(base)) return false;
    
    // Select drive
    uint8_t drive_select = 0xE0 | (disk->drive << 4) | ((lba >> 24) & 0x0F);
    ide_write(base, IDE_DRIVE_SELECT, drive_select);
    ide_delay(base);
    
    // Send write command
    ide_write(base, IDE_SECTOR_COUNT, count);
    ide_write(base, IDE_LBA_LOW, lba & 0xFF);
    ide_write(base, IDE_LBA_MID, (lba >> 8) & 0xFF);
    ide_write(base, IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    ide_write(base, IDE_COMMAND, IDE_CMD_WRITE_SECTORS);
    
    // Write data
    const uint16_t* buf = (const uint16_t*)buffer;
    for (int sector = 0; sector < count; sector++) {
        // Wait for data ready
        if (!disk_wait_data(base)) return false;
        
        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            ide_write_word(base, buf[sector * 256 + i]);
        }
        
        // Delay after write
        ide_delay(base);
    }
    
    // Flush cache
    ide_write(base, IDE_COMMAND, IDE_CMD_CACHE_FLUSH);
    disk_poll(base);
    
    return true;
}

void disk_print_info(uint32_t device_id) {
    if (device_id >= disk_count) {
        printf("Disk %u: Not found\n", device_id);
        return;
    }
    
    disk_device_t* disk = &disks[device_id];
    printf("Disk %u: %s %s\n", device_id,
           disk->bus == 0 ? "Primary" : "Secondary",
           disk->drive == 0 ? "Master" : "Slave");
    printf("  Sectors: %u (%u MB)\n", disk->sectors, disk->sectors / 2048);
    printf("  C/H/S: %u/%u/%u\n", disk->cylinders, disk->heads, disk->sectors_per_track);
    printf("  LBA48: %s\n", disk->lba48 ? "Yes" : "No");
}