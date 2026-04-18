#ifndef _KERNEL_DISK_H
#define _KERNEL_DISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Disk device types
#define DISK_TYPE_NONE      0
#define DISK_TYPE_IDE       1
#define DISK_TYPE_ATA       2
#define DISK_TYPE_SATA      3

// IDE Controller Ports
#define IDE_PRIMARY_BASE    0x1F0
#define IDE_SECONDARY_BASE  0x170

#define IDE_DATA        0x0
#define IDE_ERROR       0x1
#define IDE_FEATURES    0x1
#define IDE_SECTOR_COUNT 0x2
#define IDE_LBA_LOW     0x3
#define IDE_LBA_MID     0x4
#define IDE_LBA_HIGH    0x5
#define IDE_DRIVE_SELECT 0x6
#define IDE_COMMAND     0x7
#define IDE_STATUS      0x7
#define IDE_ALT_STATUS  0x206
#define IDE_CONTROL     0x206

// IDE Commands
#define IDE_CMD_READ_SECTORS    0x20
#define IDE_CMD_WRITE_SECTORS   0x30
#define IDE_CMD_IDENTIFY        0xEC
#define IDE_CMD_CACHE_FLUSH     0xE7

// IDE Status flags
#define IDE_STAT_ERR    0x01
#define IDE_STAT_DRQ    0x08
#define IDE_STAT_SRV    0x10
#define IDE_STAT_DF     0x20
#define IDE_STAT_RDY    0x40
#define IDE_STAT_BSY    0x80

// Disk device structure
typedef struct {
    uint32_t id;
    uint8_t type;
    uint8_t bus;        // Primary (0) or Secondary (1)
    uint8_t drive;      // Master (0) or Slave (1)
    uint32_t sectors;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors_per_track;
    bool present;
    bool lba48;
} disk_device_t;

// Initialize disk subsystem
bool disk_init(void);

// Detect disk devices
void disk_detect(void);

// Get disk device by ID
disk_device_t* disk_get_device(uint32_t id);

// Read sectors from disk (LBA28)
bool disk_read_sectors(uint32_t device_id, uint32_t lba, uint8_t count, void* buffer);

// Write sectors to disk (LBA28)
bool disk_write_sectors(uint32_t device_id, uint32_t lba, uint8_t count, const void* buffer);

// Wait for disk ready
bool disk_wait_ready(uint32_t base);

// Wait for data ready
bool disk_wait_data(uint32_t base);

// Poll until not busy
void disk_poll(uint32_t base);

// Get number of detected disks
uint32_t disk_get_count(void);

// Print disk info
void disk_print_info(uint32_t device_id);

#endif