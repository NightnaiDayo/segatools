#include "platform/system.h"

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/vfs.h"
#include "util/crc.h"
#include "util/dprintf.h"
#include "util/str.h"

#define DATA_SIZE 503
#define BLOCK_SIZE (sizeof(uint32_t) + 4 + 1 + DATA_SIZE)

#pragma pack(push, 1)

typedef struct {
    uint32_t checksum;
    char padding[6];
    uint8_t freeplay;
    char data[DATA_SIZE - 2];
} CreditBlock;

typedef struct {
    uint32_t checksum;
    char padding[4];
    uint8_t dip_switches;
    char data[DATA_SIZE];
} DipSwBlock;

typedef struct {
    CreditBlock credit_block;
    DipSwBlock dip_switch_block;
    char *data;
} SystemInfo;

#pragma pack(pop)

static SystemInfo system_info;

static struct system_config system_config;
static struct vfs_config vfs_config;

static void system_read_sysfile(const wchar_t *sys_file);
static void system_save_sysfile(const wchar_t *sys_file);

HRESULT system_init(const struct system_config *cfg,
                    const struct vfs_config *vfs_cfg) {
    HRESULT hr;
    wchar_t sys_file_path[MAX_PATH];

    assert(cfg != NULL);
    assert(vfs_cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    memcpy(&system_config, cfg, sizeof(*cfg));

    sys_file_path[0] = L'\0';
    // concatenate vfs_config.amfs with L"sysfile.dat"
    wcsncpy(sys_file_path, vfs_cfg->amfs, MAX_PATH);
    wcsncat(sys_file_path, L"\\sysfile.dat", MAX_PATH);

    system_read_sysfile(sys_file_path);

    // now write the system_config.system to the dip_switch_block
    system_save_sysfile(sys_file_path);

    return S_OK;
}

static void system_read_sysfile(const wchar_t *sys_file) {
    FILE *f = _wfopen(sys_file, L"r");

    if (f == NULL) {
        dprintf(
            "System: First run detected, system settings can only be applied "
            "AFTER the first run\n");
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size != 0x6000) {
        dprintf("System: Invalid sysfile.dat file size\n");
        fclose(f);

        return;
    }

    system_info.data = malloc(file_size);
    fread(system_info.data, 1, file_size, f);
    fclose(f);

    // copy the credit_block and dip_switch_block from the sysfile.dat
    memcpy(&system_info.credit_block, system_info.data, BLOCK_SIZE);
    memcpy(&system_info.dip_switch_block, system_info.data + 0x2800,
           BLOCK_SIZE);
}

static void system_save_sysfile(const wchar_t *sys_file) {
    char block[BLOCK_SIZE];
    uint8_t system = 0;
    uint8_t freeplay = 0;

    // open the sysfile.dat for writing in bytes mode
    FILE *f = _wfopen(sys_file, L"rb+");

    if (f == NULL) {
        return;
    }

    // write the system_config.system to the dip_switch_block
    for (int i = 0; i < 8; i++) {
        // print the dip switch configuration with labels if present
        if (system_config.dipsw_config[i].label[0] != L'\0') {
            dprintf("System: %ls: %ls\n", system_config.dipsw_config[i].label,
                    system_config.dipsw[i] ? system_config.dipsw_config[i].on
                                           : system_config.dipsw_config[i].off);
        } else if (system_config.dipsw[i]) {
            dprintf("System: DipSw%d=1 set\n", i + 1);
        }

        // set the system variable to the dip switch configuration
        if (system_config.dipsw[i]) {
            system |= (1 << i);
        }
    }

    if (system_config.freeplay) {
        // print that freeplay is enabled
        dprintf("System: Freeplay enabled\n");
        freeplay = 1;
    }

    // set the new credit block
    system_info.credit_block.freeplay = freeplay;
    // set the new dip_switch_block
    system_info.dip_switch_block.dip_switches = system;

    // calculate the new checksum, skip the old crc32 value
    // which is at the beginning of the block, thats's why the +4
    // conver the struct to chars in order for the crc32 calculation to work
    system_info.credit_block.checksum =
        crc32((char *)&system_info.credit_block + 4, BLOCK_SIZE - 4, 0);
    system_info.dip_switch_block.checksum =
        crc32((char *)&system_info.dip_switch_block + 4, BLOCK_SIZE - 4, 0);

    // build the new credit block
    memcpy(block, (char *)&system_info.credit_block, BLOCK_SIZE);

    memcpy(system_info.data, block, BLOCK_SIZE);
    memcpy(system_info.data + 0x3000, block, BLOCK_SIZE);

    // build the new dip switch block
    memcpy(block, (char *)&system_info.dip_switch_block, BLOCK_SIZE);

    memcpy(system_info.data + 0x2800, block, BLOCK_SIZE);
    memcpy(system_info.data + 0x5800, block, BLOCK_SIZE);

    // print the dip_switch_block in hex
    /*
    dprintf("System Block: ");
    for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
        dprintf("%02X ", ((uint8_t *)&system_info.dip_switch_block)[i]);
    }
    dprintf("\n");
    */

    fwrite(system_info.data, 1, 0x6000, f);

    fclose(f);
}
