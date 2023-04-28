/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// Platform-specific definitions for ZuluSCSI.
// Can be customized for different microcontrollers, this file is for GD32F205VCT6.

#pragma once

#include <gd32f20x.h>
#include <gd32f20x_gpio.h>
#include <scsi2sd.h>
#include "ZuluSCSI_config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char *g_platform_name;

#if defined(ZULUSCSI_V1_0)
#   if defined(ZULUSCSI_V1_0_mini)
#       define PLATFORM_NAME "ZuluSCSI mini v1.0"
#   else
#       define PLATFORM_NAME "ZuluSCSI v1.0"
#   endif
#   define PLATFORM_REVISION "1.0"
#   define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_ASYNC_50
#   include "ZuluSCSI_v1_0_gpio.h"
#elif defined(ZULUSCSI_V1_1)
#   define PLATFORM_NAME "ZuluSCSI v1.1"
#   define PLATFORM_REVISION "1.1"
#   define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_10
#   define PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE 4096
#   define PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE 65536
#   define PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE 8192
#   include "ZuluSCSI_v1_1_gpio.h"
#endif

#ifndef PLATFORM_VDD_WARNING_LIMIT_mV
#define PLATFORM_VDD_WARNING_LIMIT_mV 2800
#endif

// Debug logging functions
void platform_log(const char *s);

// Minimal millis() implementation as GD32F205 does not
// have an Arduino core yet.
unsigned long millis(void);
void delay(unsigned long ms);

// Precise nanosecond delays
// Works in interrupt context also, max delay 500 000 ns, min delay about 500 ns
void delay_ns(unsigned long ns);

static inline void delay_us(unsigned long us)
{
    if (us > 0)
    {
        delay_ns(us * 1000);
    }
}

// Approximate fast delay
static inline void delay_100ns()
{
    asm volatile ("nop \n nop \n nop \n nop \n nop");
}

// Initialize SPI and GPIO configuration
void platform_init();

// Initialization for main application, not used for bootloader
void platform_late_init();

// Disable the status LED
void platform_disable_led(void);

// Setup soft watchdog
void platform_reset_watchdog();

// Reinitialize SD card connection and save log from interrupt context.
// This can be used in crash handlers.
void platform_emergency_log_save();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

// This function is called by scsiPhy.cpp.
// It resets the systick counter to give 1 millisecond of uninterrupted transfer time.
// The total number of skips is kept track of to keep the correct time on average.
void SysTick_Handle_PreEmptively();

// Reprogram firmware in main program area.
#define PLATFORM_BOOTLOADER_SIZE 32768
#define PLATFORM_FLASH_TOTAL_SIZE (256 * 1024)
#define PLATFORM_FLASH_PAGE_SIZE 2048
bool platform_rewrite_flash_page(uint32_t offset, uint8_t buffer[PLATFORM_FLASH_PAGE_SIZE]);
void platform_boot_to_main_firmware();

// Configuration customizations based on DIP switch settings
// When DIPSW1 is on, Apple quirks are enabled by default.
void platform_config_hook(S2S_TargetCfg *config);
#define PLATFORM_CONFIG_HOOK(cfg) platform_config_hook(cfg)

// Write a single SCSI pin.
// Example use: SCSI_OUT(ATN, 1) sets SCSI_ATN to low (active) state.
#define SCSI_OUT(pin, state) \
    GPIO_BOP(SCSI_OUT_ ## pin ## _PORT) = (SCSI_OUT_ ## pin ## _PIN) << (state ? 16 : 0)

// Read a single SCSI pin.
// Example use: SCSI_IN(ATN), returns 1 for active low state.
#define SCSI_IN(pin) \
    ((GPIO_ISTAT(SCSI_ ## pin ## _PORT) & (SCSI_ ## pin ## _PIN)) ? 0 : 1)

// Write SCSI data bus, also sets REQ to inactive.
extern const uint32_t g_scsi_out_byte_to_bop[256];
#define SCSI_OUT_DATA(data) \
    GPIO_BOP(SCSI_OUT_PORT) = g_scsi_out_byte_to_bop[(uint8_t)(data)]

// Release SCSI data bus and REQ signal
#define SCSI_RELEASE_DATA_REQ() \
    GPIO_BOP(SCSI_OUT_PORT) = SCSI_OUT_DATA_MASK | SCSI_OUT_REQ

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    GPIO_BOP(SCSI_OUT_PORT) = SCSI_OUT_DATA_MASK | SCSI_OUT_REQ, \
    GPIO_BOP(SCSI_OUT_IO_PORT)  = SCSI_OUT_IO_PIN, \
    GPIO_BOP(SCSI_OUT_CD_PORT)  = SCSI_OUT_CD_PIN, \
    GPIO_BOP(SCSI_OUT_SEL_PORT) = SCSI_OUT_SEL_PIN, \
    GPIO_BOP(SCSI_OUT_MSG_PORT) = SCSI_OUT_MSG_PIN, \
    GPIO_BOP(SCSI_OUT_RST_PORT) = SCSI_OUT_RST_PIN, \
    GPIO_BOP(SCSI_OUT_BSY_PORT) = SCSI_OUT_BSY_PIN

// Read SCSI data bus
#define SCSI_IN_DATA(data) \
    (((~GPIO_ISTAT(SCSI_IN_PORT)) & SCSI_IN_MASK) >> SCSI_IN_SHIFT)

#ifdef __cplusplus
}

// SD card driver for SdFat
#ifndef SD_USE_SDIO
// SPI interface, ZuluSCSI v1.0
class SdSpiConfig;
extern SdSpiConfig g_sd_spi_config;
#define SD_CONFIG g_sd_spi_config
#define SD_CONFIG_CRASH g_sd_spi_config

#else
// SDIO interface, ZuluSCSI v1.1
class SdioConfig;
extern SdioConfig g_sd_sdio_config;
extern SdioConfig g_sd_sdio_config_crash;
#define SD_CONFIG g_sd_sdio_config
#define SD_CONFIG_CRASH g_sd_sdio_config_crash

#endif

// Check if a DMA request for SD card read has completed.
// This is used to optimize the timing of data transfers on SCSI bus.
bool check_sd_read_done();

#endif
