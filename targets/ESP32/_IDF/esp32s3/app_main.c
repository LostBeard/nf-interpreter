//
// Copyright (c) .NET Foundation and Contributors
// See LICENSE file in the project root for full license information.
//

#include <targetHAL.h>
#include <nanoCLR_Application.h>
#include <target_os.h>
#include <WireProtocol_ReceiverThread.h>
#include <LaunchCLR.h>
#include <string.h>

// 2026-06-21 (Riker) DEPLOY-CEILING DEBUG: the over-ceiling deploy RESETS the device on
// the final write (device alive after, blank screen, NO coredump => plain reset, not a
// panic). Stash esp_reset_reason() to the coredump partition each boot so esptool can
// read WHY it reset (brownout / RTC-WDT / SW / panic). Remove once root-caused.
#include <esp_system.h>
#include <esp_partition.h>

extern void CLRStartupThread(void const *argument);
TaskHandle_t ReceiverTask;

void receiver_task(void *pvParameter)
{
    (void)pvParameter;

    ReceiverThread(0);

    vTaskDelete(NULL);
}

// Main task start point
void main_task(void *pvParameter)
{
    (void)pvParameter;

    // CLR settings to launch CLR thread
    CLR_SETTINGS clrSettings;
    (void)memset(&clrSettings, 0, sizeof(CLR_SETTINGS));

    clrSettings.MaxContextSwitches = 50;
    clrSettings.WaitForDebugger = false;
    clrSettings.EnterDebuggerLoopAfterExit = true;

    CLRStartupThread(&clrSettings);

    vTaskDelete(NULL);
}

// Dummy defauly log method to stop output from ESP32 IDF
int dummyLog(const char *format, va_list arg)
{
    (void)format;
    (void)arg;
    return 1;
}

// App_main
// Called from Esp32 IDF start up code before scheduler starts
void app_main()
{
    // DEPLOY-CEILING DEBUG (Riker 2026-06-21): record why we (re)booted into the coredump
    // partition scratch, readable via esptool: read_flash 0x8F0000 8 -> ['R','S','T','S', reason].
    // esp_reset_reason_t: 1=POWERON 2=EXT 3=SW 4=PANIC 5=INT_WDT 6=TASK_WDT 7=WDT 9=BROWNOUT.
    {
        esp_reset_reason_t resetReason = esp_reset_reason();
        const esp_partition_t *coredumpPart =
            esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
        if (coredumpPart != NULL)
        {
            uint32_t record[2] = {0x53545352u /* 'RSTS' */, (uint32_t)resetReason};
            if (esp_partition_erase_range(coredumpPart, 0, 4096) == ESP_OK)
            {
                esp_partition_write(coredumpPart, 0, record, sizeof(record));
            }
        }
    }

    // Switch off logging so as not to interfere with WireProtocol over Uart0
    esp_log_level_set("*", ESP_LOG_NONE);

    // Stop any logging being directed to VS connection, was an issue with Nimble, outputting on Uart0
    // TODO : redirect these to debugger controlled from nanoframework.Hardware.Esp32
    esp_log_set_vprintf(dummyLog);

    ESP_ERROR_CHECK(nvs_flash_init());

    // start receiver task
    xTaskCreatePinnedToCore(&receiver_task, "ReceiverThread", 3072, NULL, 5, &ReceiverTask, 0);

    // start the CLR main task
    xTaskCreatePinnedToCore(&main_task, "main_task", 15000, NULL, 5, NULL, 0);
}
