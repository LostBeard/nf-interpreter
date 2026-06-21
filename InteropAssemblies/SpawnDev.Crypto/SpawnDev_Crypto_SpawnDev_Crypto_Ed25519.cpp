//-----------------------------------------------------------------------------
//
//    SpawnDev.Crypto - Ed25519 (RFC 8032 / SHA-512) native binding.
//    Implemented over Monocypher. (Generated skeleton from the MetadataProcessor,
//    bodies filled in. The _mshl.cpp + .h are generated and must NOT be edited.)
//
//-----------------------------------------------------------------------------

#include "SpawnDev_Crypto.h"
#include "SpawnDev_Crypto_SpawnDev_Crypto_Ed25519.h"

#include <string.h>
#include "monocypher.h"
#include "monocypher-ed25519.h"

// ESP-IDF hardware RNG (esp_hw_support). Forward-declared so this interop unit
// doesn't depend on the esp-idf include path; the symbol links from the firmware.
extern "C" void esp_fill_random(void *buf, size_t len);

using namespace SpawnDev_Crypto::SpawnDev_Crypto;

// publicKey[32] + privateKey[64] filled from a fresh hardware-random seed.
void Ed25519::GenerateKeyPair(CLR_RT_TypedArray_UINT8 param0, CLR_RT_TypedArray_UINT8 param1, HRESULT &hr)
{
    if (param0.GetSize() != 32 || param1.GetSize() != 64)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return;
    }

    uint8_t seed[32];
    esp_fill_random(seed, sizeof(seed));
    // crypto_ed25519_key_pair(secret_key[64], public_key[32], seed[32]); wipes seed.
    crypto_ed25519_key_pair(param1.GetBuffer(), param0.GetBuffer(), seed);
}

// Deterministic key pair from a 32-byte seed: publicKey[32] + privateKey[64].
void Ed25519::KeyPairFromSeed(
    CLR_RT_TypedArray_UINT8 param0, // seed[32]
    CLR_RT_TypedArray_UINT8 param1, // publicKey[32]
    CLR_RT_TypedArray_UINT8 param2, // privateKey[64]
    HRESULT &hr)
{
    if (param0.GetSize() != 32 || param1.GetSize() != 32 || param2.GetSize() != 64)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return;
    }

    // Monocypher wipes the seed buffer; copy so we don't clobber the caller's array.
    uint8_t seed[32];
    memcpy(seed, param0.GetBuffer(), 32);
    crypto_ed25519_key_pair(param2.GetBuffer(), param1.GetBuffer(), seed);
}

// signature[64] = sign(message, privateKey[64]).
void Ed25519::Sign(
    CLR_RT_TypedArray_UINT8 param0, // signature[64]
    CLR_RT_TypedArray_UINT8 param1, // privateKey[64]
    CLR_RT_TypedArray_UINT8 param2, // message
    HRESULT &hr)
{
    if (param0.GetSize() != 64 || param1.GetSize() != 64)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return;
    }

    crypto_ed25519_sign(param0.GetBuffer(), param1.GetBuffer(), param2.GetBuffer(), param2.GetSize());
}

// true if signature[64] is valid for message under publicKey[32].
bool Ed25519::Verify(
    CLR_RT_TypedArray_UINT8 param0, // signature[64]
    CLR_RT_TypedArray_UINT8 param1, // publicKey[32]
    CLR_RT_TypedArray_UINT8 param2, // message
    HRESULT &hr)
{
    if (param0.GetSize() != 64 || param1.GetSize() != 32)
    {
        hr = CLR_E_INVALID_PARAMETER;
        return false;
    }

    // crypto_ed25519_check returns 0 when the signature is valid.
    return crypto_ed25519_check(param0.GetBuffer(), param1.GetBuffer(), param2.GetBuffer(), param2.GetSize()) == 0;
}
