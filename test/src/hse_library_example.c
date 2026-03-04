/**
 * @file    hse_library_example.c
 * @brief   Example Application using HSE Library
 *
 * This example demonstrates how to use the HSE Library extracted from
 * the S32K344 Template project. It shows actual cryptographic operations
 * using the HSE host functions.
 *
 * License: BSD 3-clause
 */

#include "S32K344.h"
#include "hse_host.h"
#include "hse_interface.h"
#include "hse_demo_app_services.h"
#include "hse_host_cipher.h"
#include "hse_host_mac.h"
#include "hse_host_import_key.h"
#include "hse_keys_allocator.h"
#include <string.h>
#include <stdio.h>

/*==============================================================
 *                    EXAMPLE 1: AES Encryption
 *==============================================================*/

/**
 * @brief Example: Perform AES ECB encryption
 *
 * This function demonstrates how to encrypt data using the HSE library.
 * It mirrors the actual implementation from hse_crypto.c in the Template project.
 */
hseSrvResponse_t Example_AESEncrypt(void)
{
    hseSrvResponse_t srvResponse = HSE_SRV_RSP_GENERAL_ERROR;
    uint8_t u8MuChannel;

    // Test vectors from Template project
    const uint8_t plaintext[] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };

    const uint8_t aesKey[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xad, 0xd4, 0xfe,
        0xba, 0xfb, 0xcf, 0x45, 0x01, 0x13, 0x32, 0x66
    };

    uint8_t ciphertext[16];

    // Get a free channel
    u8MuChannel = HSE_GetFreeChannel(MU0);
    if (HSE_INVALID_CHANNEL == u8MuChannel) {
        return HSE_SRV_RSP_GENERAL_ERROR;
    }

    // Get service descriptor
    hseSrvDescriptor_t* pHseSrvDesc = gHseSrvDesc[MU0];
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    // Configure AES cipher service
    pHseSrvDesc->srvId = HSE_SRV_CIPHER;
    pHseSrvDesc->srvPayload.cipherReq.cipherKeyHandle = PTR_TO_HOST_ADDR(aesKey);
    pHseSrvDesc->srvPayload.cipherReq.cipherAlgo = HSE_CIPHER_ALGO_AES;
    pHseSrvDesc->srvPayload.cipherReq.cipherMode = HSE_CIPHER_MODE_ECB;
    pHseSrvDesc->srvPayload.cipherReq.cipherDir = HSE_CIPHER_DIR_ENCRYPT;
    pHseSrvDesc->srvPayload.cipherReq.inputLength = sizeof(plaintext);
    pHseSrvDesc->srvPayload.cipherReq.pIn = PTR_TO_HOST_ADDR(plaintext);
    pHseSrvDesc->srvPayload.cipherReq.pOut = PTR_TO_HOST_ADDR(ciphertext);

    // Send request to HSE
    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);

    if (HSE_SRV_RSP_OK == srvResponse) {
        // Encryption successful - ciphertext is ready
        printf("AES Encryption: OK\n");
        // Print ciphertext if desired
    }

    return srvResponse;
}

/*==============================================================
 *                    EXAMPLE 2: SHA-256 Hash
 *==============================================================*/

/**
 * @brief Example: Compute SHA-256 hash
 *
 * This function shows how to compute a hash using the HSE library.
 */
hseSrvResponse_t Example_SHA256Hash(void)
{
    hseSrvResponse_t srvResponse = HSE_SRV_RSP_GENERAL_ERROR;
    uint8_t u8MuChannel;

    const uint8_t message[] = "The quick brown fox jumps over the lazy dog.";
    uint8_t hash[32];

    // Get free channel
    u8MuChannel = HSE_GetFreeChannel(MU0);
    if (HSE_INVALID_CHANNEL == u8MuChannel) {
        return HSE_SRV_RSP_GENERAL_ERROR;
    }

    // Get service descriptor
    hseSrvDescriptor_t* pHseSrvDesc = gHseSrvDesc[MU0];
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    // Configure hash service
    pHseSrvDesc->srvId = HSE_SRV_HASH;
    pHseSrvDesc->srvPayload.hashReq.hashAlgo = HSE_HASH_ALGO_SHA2_256;
    pHseSrvDesc->srvPayload.hashReq.inputLength = sizeof(message) - 1;  // Exclude null terminator
    pHseSrvDesc->srvPayload.hashReq.pIn = PTR_TO_HOST_ADDR((uint8_t*)message);
    pHseSrvDesc->srvPayload.hashReq.pHashLength = PTR_TO_HOST_ADDR(hash);

    // Send request
    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);

    if (HSE_SRV_RSP_OK == srvResponse) {
        printf("SHA-256 Hash: OK\n");
        // Hash is now in 'hash' buffer
    }

    return srvResponse;
}

/*==============================================================
 *                    EXAMPLE 3: CMAC Generation
 *==============================================================*/

/**
 * @brief Example: Generate CMAC tag
 *
 * This function demonstrates CMAC (Cipher-based MAC) generation.
 */
hseSrvResponse_t Example_CMACGenerate(void)
{
    hseSrvResponse_t srvResponse = HSE_SRV_RSP_GENERAL_ERROR;
    uint8_t u8MuChannel;

    const uint8_t message[] = "Message for authentication";
    const uint8_t key[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xad, 0xd4, 0xfe,
        0xba, 0xfb, 0xcf, 0x45, 0x01, 0x13, 0x32, 0x66
    };
    uint8_t tag[16];

    // Get free channel
    u8MuChannel = HSE_GetFreeChannel(MU0);
    if (HSE_INVALID_CHANNEL == u8MuChannel) {
        return HSE_SRV_RSP_GENERAL_ERROR;
    }

    // Get service descriptor
    hseSrvDescriptor_t* pHseSrvDesc = gHseSrvDesc[MU0];
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    // Configure MAC service
    pHseSrvDesc->srvId = HSE_SRV_MAC;
    pHseSrvDesc->srvPayload.macReq.macAlgo = HSE_MAC_ALGO_CMAC;
    pHseSrvDesc->srvPayload.macReq.macKeyHandle = PTR_TO_HOST_ADDR((uint8_t*)key);
    pHseSrvDesc->srvPayload.macReq.inputLength = sizeof(message) - 1;
    pHseSrvDesc->srvPayload.macReq.pIn = PTR_TO_HOST_ADDR((uint8_t*)message);
    pHseSrvDesc->srvPayload.macReq.pTag = PTR_TO_HOST_ADDR(tag);
    pHseSrvDesc->srvPayload.macReq.tagLength = sizeof(tag);

    // Send request
    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);

    if (HSE_SRV_RSP_OK == srvResponse) {
        printf("CMAC Generation: OK\n");
        // Tag is now available in 'tag' buffer
    }

    return srvResponse;
}

/*==============================================================
 *                    MAIN APPLICATION
 *==============================================================*/

/**
 * @brief Main entry point
 *
 * Demonstrates how to use the HSE Library in a complete application.
 */
int main(void)
{
    hseSrvResponse_t response;

    printf("\n=================================\n");
    printf("HSE Library Examples\n");
    printf("=================================\n\n");

    // Example 1: AES Encryption
    printf("Running Example 1: AES Encryption...\n");
    response = Example_AESEncrypt();
    if (HSE_SRV_RSP_OK != response) {
        printf("AES Encryption FAILED: 0x%x\n", response);
    }
    printf("\n");

    // Example 2: SHA-256 Hash
    printf("Running Example 2: SHA-256 Hash...\n");
    response = Example_SHA256Hash();
    if (HSE_SRV_RSP_OK != response) {
        printf("SHA-256 Hash FAILED: 0x%x\n", response);
    }
    printf("\n");

    // Example 3: CMAC Generation
    printf("Running Example 3: CMAC Generation...\n");
    response = Example_CMACGenerate();
    if (HSE_SRV_RSP_OK != response) {
        printf("CMAC Generation FAILED: 0x%x\n", response);
    }

    printf("\n=================================\n");
    printf("Examples completed\n");
    printf("=================================\n\n");

    // Application continues...
    for (;;) {
        // Main loop
    }

    return 0;
}

/*==============================================================
 *                    NOTE ON USAGE
 *==============================================================
 *
 * This example shows the basic pattern for using HSE services:
 *
 * 1. Get a free channel: HSE_GetFreeChannel(MU_INSTANCE)
 * 2. Get service descriptor: gHseSrvDesc[MU_INSTANCE]
 * 3. Clear and configure the descriptor
 * 4. Set service ID: pHseSrvDesc->srvId = HSE_SRV_*
 * 5. Set service-specific parameters
 * 6. Send request: HSE_Send(mu_instance, channel, options, descriptor)
 * 7. Check response: if (HSE_SRV_RSP_OK == response) { ... }
 *
 * For more examples, see:
 * - lib/services/fw_crypto/hse_crypto.c (comprehensive examples)
 * - lib/host_hse/hse_host.h (API documentation)
 * - lib/services/inc/hse_demo_app_services.h (service definitions)
 */
