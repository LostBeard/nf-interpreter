// 2026-05-05: C-callable wrapper around CLR_Debug::Printf so the C-only
// Target_BlockStorage_ESP32FlashDriver.c can route diagnostic output
// through the wire-protocol debug channel (visible in nf-deploy.cs's
// captured runtime output) instead of the silenced ESP-IDF console
// (CONFIG_LOG_DEFAULT_LEVEL_NONE=y on this firmware build).
//
// Usage from C:
//   extern "C" void Esp32FlashDriver_DiagPrintf(const char* fmt, ...);
//
// Format follows printf(3) - same convention as CLR_Debug::Printf.

#include <stdarg.h>
#include <nanoCLR_Runtime.h>

extern "C" void Esp32FlashDriver_DiagPrintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    CLR_Debug::PrintfV(fmt, args);
    va_end(args);
}
