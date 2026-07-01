//-----------------------------------------------------------------------------
//
//                   ** WARNING! ** 
//    This file was generated automatically by a tool.
//    Re-running the tool will overwrite this file.
//    You should copy this file to a custom location
//    before adding any customization in the copy to
//    prevent loss of your changes when the tool is
//    re-run.
//
//-----------------------------------------------------------------------------

#ifndef SPAWNDEV_WEBRTC_SPAWNDEV_WEBRTC_SPAWNDEV_WEBRTC_NATIVETEXT_H
#define SPAWNDEV_WEBRTC_SPAWNDEV_WEBRTC_SPAWNDEV_WEBRTC_NATIVETEXT_H

namespace SpawnDev_WebRTC
{
    namespace SpawnDev_WebRTC
    {
        struct NativeText
        {
            // Helper Functions to access fields of managed object
            // Declaration of stubs. These functions are implemented by Interop code developers

            static signed int CreateFont( CLR_RT_TypedArray_UINT8 param0, HRESULT &hr );

            static signed int MeasureText( signed int param0, const char* param1, HRESULT &hr );

            static signed int FontHeight( signed int param0, HRESULT &hr );

            static signed int RenderText( signed int param0, const char* param1, signed int param2, CLR_RT_TypedArray_UINT8 param3, HRESULT &hr );

            static void ReleaseFont( signed int param0, HRESULT &hr );

        };
    }
}

#endif // SPAWNDEV_WEBRTC_SPAWNDEV_WEBRTC_SPAWNDEV_WEBRTC_NATIVETEXT_H
