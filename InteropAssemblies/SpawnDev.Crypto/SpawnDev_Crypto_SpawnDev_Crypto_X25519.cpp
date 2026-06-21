//-----------------------------------------------------------------------------
//
//    SpawnDev.Crypto - X25519 (Curve25519 ECDH) native binding.
//    Implemented over Monocypher.
//
//-----------------------------------------------------------------------------

#include "SpawnDev_Crypto.h"
#include "SpawnDev_Crypto_SpawnDev_Crypto_X25519.h"

#include "monocypher.h"

// ESP-IDF hardware RNG (esp_hw_support), forward-declared (see Ed25519 binding).
extern "C" void esp_fill_random(void *buf, size_t len);

using namespace SpawnDev_Crypto::SpawnDev_Crypto;

// privateKey[32] = 32 fresh hardware-random bytes (Monocypher clamps on use).
void X25519::GeneratePrivateKey(CLR_RT_TypedArray_UINT8 param0, HRESULT &hr)
{
    if (param0.GetSize() != 32)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return;
    }

    esp_fill_random(param0.GetBuffer(), 32);
}

// publicKey[32] = X25519 base * privateKey[32].
void X25519::GetPublicKey(
    CLR_RT_TypedArray_UINT8 param0, // publicKey[32]
    CLR_RT_TypedArray_UINT8 param1, // privateKey[32]
    HRESULT &hr)
{
    if (param0.GetSize() != 32 || param1.GetSize() != 32)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return;
    }

    crypto_x25519_public_key(param0.GetBuffer(), param1.GetBuffer());
}

// sharedSecret[32] = X25519(privateKey[32], theirPublicKey[32]).
void X25519::SharedSecret(
    CLR_RT_TypedArray_UINT8 param0, // sharedSecret[32]
    CLR_RT_TypedArray_UINT8 param1, // privateKey[32]
    CLR_RT_TypedArray_UINT8 param2, // theirPublicKey[32]
    HRESULT &hr)
{
    if (param0.GetSize() != 32 || param1.GetSize() != 32 || param2.GetSize() != 32)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return;
    }

    crypto_x25519(param0.GetBuffer(), param1.GetBuffer(), param2.GetBuffer());
}
