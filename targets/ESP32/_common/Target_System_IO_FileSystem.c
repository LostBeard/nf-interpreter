//
// Copyright (c) .NET Foundation and Contributors
// See LICENSE file in the project root for full license information.
//

//
//	Initialise SD card and mount on VFS
//  Supports  MMC and SPI SD cards
//  MMC supports 4bit or 1 bit

// == Boards ==
// OLimex EVB - 1 bit SD/MMC  ( DAT0 - GPIO2, CMD/DI - GPIO15 ), No Card detect, No Write protect
// Wrover V4 -  4 bit SD/MMC  ( Card detect )
//              D0 = 2, D1 = 4, D2 = 12, D3 = 13
//              CLK = 14, CMD = 15, DETECT = 21

//  SPI test
//  MISO = 2, MOSI = 15, CLK = 14, CS = 15

//   5    	VSPICS0 (cs)
//	18		VSPICLK (clockPin) *
//  19      VSPIQ   (miso) *
//  21		VSPIHD  (HD)
//	22		VSPIWP  (WP)
//	23		VSPID   (mosi) *

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp32_idf.h>
#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <target_platform.h>

#include <nanoHAL_System_IO_FileSystem.h>
#include <Esp32_DeviceMapping.h>

#if (HAL_USE_SDC == TRUE)

static const char *TAG = "SDCard";

sdmmc_card_t *card;

// 2026-05-04: surface SD-mount diagnostics through the wire-protocol debug
// channel since CONFIG_LOG_DEFAULT_LEVEL_NONE=y silences ESP_LOG output.
// Implementation in Target_System_IO_FileSystem_Diag.cpp.
extern void Storage_DiagPrintf(const char *fmt, ...);

//
//  Unmount SD card ( MMC/SDIO or SPI)
//
bool Storage_UnMountSDCard(int driveIndex)
{
    char mountPoint[] = INDEX0_DRIVE_LETTER;

    // Change fatfs drive letter to mount point  D: -> /D for ESP32 VFS
    mountPoint[1] = mountPoint[0] + driveIndex;
    mountPoint[0] = '/';

    if (esp_vfs_fat_sdcard_unmount(mountPoint, card) != ESP_OK)
    {
        return false;
    }

    card = NULL;

    return true;
}

bool LogMountResult(esp_err_t errCode)
{
    if (errCode != ESP_OK)
    {
        if (errCode == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
            Storage_DiagPrintf("[SDCard] mount failed: ESP_FAIL (FATFS f_mount said no valid volume)\r\n");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s).  ", esp_err_to_name(errCode));
            Storage_DiagPrintf("[SDCard] mount failed: errCode=0x%x (%s)\r\n", (unsigned int)errCode, esp_err_to_name(errCode));
        }
        return false;
    }
    return true;
}

#if SOC_SDMMC_HOST_SUPPORTED
//
// Mount SDcard on MMC/SDIO bus
//
//  bit1Mode- true to use 1 bit MMC interface
//  driveIndex =  0 = first drive, 1 = 2nd drive
//
bool Storage_MountMMC(bool bit1Mode, int driveIndex)
{
    esp_err_t errCode;
    char mountPoint[] = INDEX0_DRIVE_LETTER;

    // Change fatfs drive letter to mount point  D: -> /D for ESP32 VFS or /E
    mountPoint[1] = mountPoint[0] + driveIndex;
    mountPoint[0] = '/';

    ESP_LOGI(TAG, "Initializing SDMMC%d SD card", driveIndex + 1);
    Storage_DiagPrintf("[SDCard] Storage_MountMMC: drive=%c bit1Mode=%d driveIndex=%d\r\n",
        mountPoint[1], (int)bit1Mode, driveIndex);

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Set bus width and pins to use
#if (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4))
    slot_config.clk = (gpio_num_t)Esp32_GetSDmmcDevicePins_C(driveIndex, Esp32SdmmcPin_Clock);
    slot_config.cmd = (gpio_num_t)Esp32_GetSDmmcDevicePins_C(driveIndex, Esp32SdmmcPin_Command);
    slot_config.d0 = (gpio_num_t)Esp32_GetSDmmcDevicePins_C(driveIndex, Esp32SdmmcPin_D0);
#endif

    if (bit1Mode)
    {
        slot_config.width = 1;
    }
    else
    {
        slot_config.width = 4;
#if (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4))
        slot_config.d1 = (gpio_num_t)Esp32_GetSDmmcDevicePins_C(driveIndex, Esp32SdmmcPin_D1);
        slot_config.d2 = (gpio_num_t)Esp32_GetSDmmcDevicePins_C(driveIndex, Esp32SdmmcPin_D2);
        slot_config.d3 = (gpio_num_t)Esp32_GetSDmmcDevicePins_C(driveIndex, Esp32SdmmcPin_D3);
#endif
    }

    // from IDF readme on SDMMC
    //
    ///////////////////////////////
    // Pin assignments for ESP32 //
    ///////////////////////////////
    // On ESP32, SDMMC peripheral is connected to specific GPIO pins using the IO MUX.
    // GPIO pins cannot be customized. Please see the table below for the pin connections.

    // ESP32 pin     | SD card pin | Notes
    // --------------|-------------|------------
    // GPIO14 (MTMS) | CLK         | 10k pullup in SD mode
    // GPIO15 (MTDO) | CMD         | 10k pullup in SD mode
    // GPIO2         | D0          | 10k pullup in SD mode, pull low to go into download mode
    // GPIO4         | D1          | not used in 1-line SD mode; 10k pullup in 4-line SD mode
    // GPIO12 (MTDI) | D2          | not used in 1-line SD mode; 10k pullup in 4-line SD mode
    // GPIO13 (MTCK) | D3          | not used in 1-line SD mode, but card's D3 pin must have a 10k pullup

    //////////////////////////////////
    // Pin assignments for ESP32-S3 //
    //////////////////////////////////
    // On ESP32-S3, SDMMC peripheral is connected to GPIO pins using GPIO matrix.
    // This allows arbitrary GPIOs to be used to connect an SD card. In this example, GPIOs can be configured in two
    // ways:

    // The table below lists the default pin assignments.

    // ESP32-S3 pin  | SD card pin | Notes
    // --------------|-------------|------------
    // GPIO36        | CLK         | 10k pullup
    // GPIO35        | CMD         | 10k pullup
    // GPIO37        | D0          | 10k pullup
    // GPIO38        | D1          | not used in 1-line SD mode; 10k pullup in 4-line mode
    // GPIO33        | D2          | not used in 1-line SD mode; 10k pullup in 4-line mode
    // GPIO34        | D3          | not used in 1-line SD mode, but card's D3 pin must have a 10k pullup

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    //	Mount the SDCard device as a FAT device on the VFS
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = SDC_MAX_OPEN_FILES,
        .allocation_unit_size = 16 * 1024};

    errCode = esp_vfs_fat_sdmmc_mount(mountPoint, &host, &slot_config, &mount_config, &card);
    if (errCode == ESP_ERR_INVALID_STATE)
    {
        // Invalid state means its already mounted, this can happen if you are trying to debug mount from managed code
        // and the code has already run & mounted
        Storage_UnMountSDCard(driveIndex);
        errCode = esp_vfs_fat_sdmmc_mount(mountPoint, &host, &slot_config, &mount_config, &card);
    }

    return LogMountResult(errCode);
}
#endif

//
// Mount card on SPI bus
// Expects SPI bus to be already initialised
//
//  spiBus     -  SPI bus index (0 based)
//  csPin      - Chip select pin for SD card
//  driveIndex - 0 = first drive
//
//  return true if OK
//
bool Storage_MountSpi(int spiBus, uint32_t csPin, int driveIndex)
{
    esp_err_t errCode;
    char mountPoint[] = INDEX0_DRIVE_LETTER;

    // Change fatfs drive letter to mount point  D: -> /D for ESP32 VFS
    mountPoint[1] = mountPoint[0] + driveIndex;
    mountPoint[0] = '/';

    ESP_LOGI(TAG, "Initializing SPI SD card");
    Storage_DiagPrintf("[SDCard] Storage_MountSpi: spiBus=%d csPin=%d drive=%c host.slot=%d\r\n",
        spiBus, (int)csPin, mountPoint[1], spiBus + SPI2_HOST);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

#if defined(CONFIG_IDF_TARGET_ESP32S2)
    host.slot = spiBus + SPI3_HOST;
#else
    // on all others
    host.slot = spiBus + SPI2_HOST;
#endif

    // 2026-06-20 SpawnWear watch (ESP32-S3-Touch-AMOLED-2.06): cap the SDSPI clock. The SD
    // slot is wired for SDMMC (no dedicated SPI bus pull-ups) and SD "SPI mode" is optional
    // in the spec, so the link margin is modest. 400 kHz is proven rock-solid here; 4 MHz is
    // the throughput target (Media Player) now under on-hardware test. If a known-good card
    // garbles block reads at 4 MHz, step back toward 400 kHz. (An old 960 MB card has a flaky
    // SPI bulk read at every speed - test throughput with a good card, not that one.)
    host.max_freq_khz = 4000;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)csPin;
    slot_config.host_id = (spi_host_device_t)host.slot;

    // Free a mount leaked by a prior CLR *soft* reboot. A ClrOnly reboot (what
    // nf-deploy / VS use) re-runs the CLR WITHOUT a hardware reset, so this `card`
    // global and its VFS/FATFS/sdspi allocations survive from the previous run.
    // esp_vfs_fat_sdspi_mount below would allocate a fresh set on top, leaking
    // DMA-capable RAM each soft reboot until the mount starves (ESP_ERR_NO_MEM).
    // Unmount + free the stale one first. On a cold boot `card` is NULL (BSS
    // zeroed) so this is a no-op. The SPI bus is left intact - it's reused.
    if (card != NULL)
    {
        Storage_DiagPrintf("[SDCard] stale card=%p from prior soft reboot - unmounting + freeing\r\n", (void *)card);
        Storage_UnMountSDCard(driveIndex);
    }
    else
    {
        Storage_DiagPrintf("[SDCard] card=NULL at entry (cold boot or already freed)\r\n");
    }

    // 2026-06-21 diagnostic: the SDSPI mount intermittently fails with
    // ESP_ERR_NO_MEM (0x101) after a warm reboot - the driver needs a contiguous
    // DMA-capable internal-RAM block (bounce buffer) that isn't always available.
    // Log the DMA/internal heap picture right before mounting so we can SEE how
    // tight it is on a failing vs succeeding boot.
    Storage_DiagPrintf(
        "[SDCard] heap before mount: DMA free=%u largest=%u | INTERNAL free=%u largest=%u\r\n",
        (unsigned int)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    // Warm-up retry: the first SDSPI card-init after power-on is often flaky on this
    // SDMMC-wired slot (the very first f_mount returns ESP_FAIL, a subsequent one
    // succeeds). Retry a few times here with a short settle so a transient warm-up
    // failure does not surface as a scary "mount failed" log - LogMountResult below
    // logs only the FINAL outcome.
    for (int attempt = 0; attempt < 4; attempt++)
    {
        errCode = esp_vfs_fat_sdspi_mount(mountPoint, &host, &slot_config, &mount_config, &card);
        if (errCode == ESP_OK)
        {
            break;
        }
        if (errCode == ESP_ERR_INVALID_STATE)
        {
            // Already mounted (e.g. a re-mount from managed debug code) - unmount + retry.
            Storage_UnMountSDCard(driveIndex);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    return LogMountResult(errCode);
}

#endif
