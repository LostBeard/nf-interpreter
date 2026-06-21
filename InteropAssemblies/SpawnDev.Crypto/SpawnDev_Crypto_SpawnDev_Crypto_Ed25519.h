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

#ifndef SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_ED25519_H
#define SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_ED25519_H

namespace SpawnDev_Crypto
{
    namespace SpawnDev_Crypto
    {
        struct Ed25519
        {
            // Helper Functions to access fields of managed object
            // Declaration of stubs. These functions are implemented by Interop code developers

            static void GenerateKeyPair( CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, HRESULT &hr );

            static void KeyPairFromSeed( CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, CLR_RT_TypedArray_UINT8 param2, HRESULT &hr );

            static void Sign( CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, CLR_RT_TypedArray_UINT8 param2, HRESULT &hr );

            static bool Verify( CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, CLR_RT_TypedArray_UINT8 param2, HRESULT &hr );

        };
    }
}

#endif // SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_SPAWNDEV_CRYPTO_ED25519_H
