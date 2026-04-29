//
// Copyright (c) .NET Foundation and Contributors
// See LICENSE file in the project root for full license information.
//

#ifndef DISPLAY_INTERFACE_H
#define DISPLAY_INTERFACE_H

#include "nanoCLR_Types.h"
#include "nanoCLR_Interop.h"
#include "Core.h"

// Display configuration
struct DisplayInterfaceConfig
{
    union {
        struct
        {
            CLR_UINT8 spiBus;
            CLR_INT32 chipSelect;
            CLR_INT32 dataCommand;
            CLR_INT32 reset;
            CLR_INT32 backLight;
        } Spi;
        struct
        {
            CLR_UINT8 spiBus;             // ESP32 SPI host index (0 = SPI2_HOST, 1 = SPI3_HOST). Other hosts: future.
            CLR_INT32 chipSelect;         // CS GPIO.
            CLR_INT32 sclk;               // SPI clock GPIO.
            CLR_INT32 dataLine0;          // QSPI data line 0 (also MOSI in single-line mode).
            CLR_INT32 dataLine1;          // QSPI data line 1.
            CLR_INT32 dataLine2;          // QSPI data line 2 (data2 / quadwp).
            CLR_INT32 dataLine3;          // QSPI data line 3 (data3 / quadhd).
            CLR_INT32 reset;              // Display reset GPIO. -1 = no hardware reset (chip is software-reset only).
            CLR_INT32 backLight;          // Backlight enable GPIO. -1 = software-controlled brightness via panel register.
            // No DataCommand pin - QSPI panels (CO5300, AXS15231B, RM67162, ...) encode
            // command vs data in the SPI transaction's command byte itself.
        } Qspi;
        struct
        {
            CLR_INT8 i2cBus;
            CLR_INT8 address;
            CLR_INT8 fastMode;
        } I2c;
        struct
        {
            CLR_INT16 enable;
            CLR_INT16 control;
            CLR_INT16 backlight;
            CLR_INT16 Horizontal_synchronization;
            CLR_INT16 Horizontal_back_porch;
            CLR_INT16 Horizontal_front_porch;
            CLR_INT16 Vertical_synchronization;
            CLR_INT16 Vertical_back_porch;
            CLR_INT16 Vertical_front_porch;
            CLR_INT16 Frequency_Divider;
        } VideoDisplay;
    };
    struct
    {
        CLR_UINT16 x;
        CLR_UINT16 y;
        CLR_UINT16 width;
        CLR_UINT16 height;
    } Screen;
    struct
    {
        CLR_UINT32 Width;
        CLR_UINT32 Height;
        CLR_UINT8 BitsPerPixel;
        CLR_RT_HeapBlock_Array *InitializationSequence;
        CLR_UINT8 MemoryWrite;
        CLR_UINT8 SetColumnAddress;
        CLR_UINT8 SetRowAddress;
        CLR_RT_HeapBlock_Array *PowerModeNormal;
        CLR_RT_HeapBlock_Array *PowerModeSleep;
        CLR_RT_HeapBlock_Array *OrientationPortrait;
        CLR_RT_HeapBlock_Array *OrientationPortrait180;
        CLR_RT_HeapBlock_Array *OrientationLandscape;
        CLR_RT_HeapBlock_Array *OrientationLandscape180;
        CLR_RT_HeapBlock_Array *Clear;
        CLR_UINT8 Brightness;
        CLR_UINT8 DefaultOrientation;
        CLR_UINT8 SetWindowType;
        // ---------------------------------------------------------------------
        // QSPI display extensions. Populated by the managed GraphicDriver descriptor;
        // ignored when BusType == 0 (the standard SPI path).
        // ---------------------------------------------------------------------
        CLR_UINT8 BusType;                  // 0 = Spi (default, with DC pin), 1 = Qspi (hybrid 1-line cmd/addr, 4-line data).
        CLR_UINT8 QspiRegisterWriteCommand; // SPI cmd byte that prefixes register-write transactions (0x02 for CO5300).
        CLR_UINT8 QspiMemoryWriteCommand;   // SPI cmd byte that prefixes memory-write (pixel) transactions (0x32 for CO5300).
        CLR_UINT32 QspiMemoryWriteAddress;  // 24-bit address phase that accompanies the memory-write (0x003C00 for CO5300).
    } GenericDriverCommands;
};

struct DisplayInterface
{
    void Initialize(DisplayInterfaceConfig &config);
    void GetTransferBuffer(CLR_UINT8 *&BufferAddress, CLR_UINT32 &sizeInBytes);
    void ClearFrameBuffer();
    void WriteToFrameBuffer(CLR_UINT8 command, CLR_UINT8 data[], CLR_UINT32 dataCount, CLR_UINT32 frameOffset = 0);
    void DisplayBacklight(bool on); // true = on
    void SendCommand(CLR_UINT8 arg_count, ...);
    void SendBytes(CLR_UINT8 *data, CLR_UINT32 length);
    void SendData16Windowed(
        CLR_UINT16 *data,
        CLR_UINT32 startX,
        CLR_UINT32 startY,
        CLR_UINT32 width,
        CLR_UINT32 height,
        CLR_UINT32 stride,
        bool doByteSwap);
    void FillData16(CLR_UINT16 fillValue, CLR_UINT32 fillLength);
    void SetCommandMode(int mode);
};

#endif // DISPLAY_INTERFACE_H
