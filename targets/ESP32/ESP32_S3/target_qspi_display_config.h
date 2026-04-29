//
// Copyright (c) .NET Foundation and Contributors
// See LICENSE file in the project root for full license information.
//
// Target-local QSPI display pin map. Used by Qspi_To_Display.cpp on builds where
// CONFIG_GRAPHICS_IFACE_QSPI=y. The pins below match the Waveshare ESP32-S3-Touch-
// AMOLED-2.06 watch (the first board this driver targets); other QSPI watches
// using ESP32_S3_BLE_QSPI as their preset can override this header in a target-
// local nanoCLR/ subdirectory.
//
// LONG-TERM: this header goes away once managed nanoFramework.UI.QspiConfiguration
// + NativeInitQspi land. Then pin assignments come from the descriptor.
//

#ifndef TARGET_QSPI_DISPLAY_CONFIG_H
#define TARGET_QSPI_DISPLAY_CONFIG_H

// QSPI bus host: 0 = SPI2_HOST, 1 = SPI3_HOST.
// On the Waveshare 2.06 watch we put the display on SPI2 to leave SPI3 free for SD.
#define QSPI_DISPLAY_HOST 0

// Pin numbers per the Waveshare ESP32-S3-Touch-AMOLED-2.06 schematic
// (cross-checked against the Rust port at github.com/infinition/waveshare-watch-rs).
#define QSPI_DISPLAY_SCLK 11
#define QSPI_DISPLAY_CS   12
#define QSPI_DISPLAY_D0   4   // SDIO0 / MOSI in single-line mode
#define QSPI_DISPLAY_D1   5   // SDIO1
#define QSPI_DISPLAY_D2   6   // SDIO2 / quadwp
#define QSPI_DISPLAY_D3   7   // SDIO3 / quadhd
#define QSPI_DISPLAY_RST  8
#define QSPI_DISPLAY_BL   -1  // No dedicated backlight pin; brightness via panel register 0x51.

#endif // TARGET_QSPI_DISPLAY_CONFIG_H
