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

#ifndef SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_X25519_H
#define SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_X25519_H

namespace SpawnDev_Crypto
{
    namespace SpawnDev_Crypto
    {
        struct X25519
        {
            // Helper Functions to access fields of managed object
            // Declaration of stubs. These functions are implemented by Interop code developers

            static void GeneratePrivateKey( CLR_RT_TypedArray_UINT8 param0, HRESULT &hr );

            static void GetPublicKey( CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, HRESULT &hr );

            static void SharedSecret( CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, CLR_RT_TypedArray_UINT8 param2, HRESULT &hr );

        };
    }
}

#endif // SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_X25519_H
