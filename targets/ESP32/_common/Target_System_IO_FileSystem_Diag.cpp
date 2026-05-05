// 2026-05-04: C-callable wrapper around CLR_Debug::Printf so the C-only
// Target_System_IO_FileSystem.c can route SD-mount diagnostic output
// through the wire-protocol debug channel (visible in nf-deploy.cs's
// captured runtime output) instead of the silenced ESP-IDF console
// (CONFIG_LOG_DEFAULT_LEVEL_NONE=y on this firmware build).
//
// Mirrors the Esp32FlashDriver_Diag pattern. Used to surface the
// actual ESP-IDF error code from esp_vfs_fat_sdspi_mount /
// esp_vfs_fat_sdmmc_mount when SD card mount fails - without it the
// only visible symptom is a generic CLR_E_VOLUME_NOT_FOUND from
// MountNative with no actionable detail.
//
// Usage from C:
//   extern "C" void Storage_DiagPrintf(const char* fmt, ...);
//
// Format follows printf(3) - same convention as CLR_Debug::Printf.

#include <stdarg.h>
#include <nanoCLR_Runtime.h>

extern "C" void Storage_DiagPrintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    CLR_Debug::PrintfV(fmt, args);
    va_end(args);
}
