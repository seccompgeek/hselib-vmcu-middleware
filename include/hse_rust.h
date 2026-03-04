/**
 * @file   hse_rust.h
 * @brief  C declarations for functions implemented in the Rust component
 *         (rust/src/lib.rs → libhselib_rust.a).
 *
 * Add a declaration here for every `#[no_mangle] pub extern "C"` function
 * that Rust code exposes to the rest of the C library.
 */

#ifndef HSE_RUST_H
#define HSE_RUST_H

#include <stdbool.h>
#include <stdint.h>
#include "hse_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Placeholder function — returns the bitwise NOT of @p value.
 *
 * Implemented in rust/src/lib.rs.  Replace with real Rust functionality
 * as it is migrated from C.
 *
 * @param  value  Input value.
 * @return        Bitwise NOT of @p value.
 */
uint32_t hse_rust_example(uint32_t value);

/**
 * @brief  Fill @p buf with @p len bytes of DRG4-class random data from the
 *         HSE hardware RNG.
 *
 * Implemented in rust/src/lib.rs.  Internally calls the C function
 * GetRngDRG4Num() — this is an example of a Rust function that calls back
 * into the C side of the library.
 *
 * @param  buf  Caller-supplied output buffer (must not be NULL).
 * @param  len  Number of random bytes to generate (must be > 0).
 * @return      true on success, false if the HSE returned an error.
 */
bool hse_rust_fill_random_drg4(uint8_t *buf, uint32_t len);

/**
 * @brief  Rust wrapper for HSE_Send with bounds checking.
 *
 * Validates that @p u8MuInstance < HSE_NUM_OF_MU_INSTANCES,
 * @p u8MuChannel < HSE_NUM_OF_CHANNELS_PER_MU, and @p pHseSrvDesc is not NULL
 * before forwarding to HSE_Send.  Returns HSE_SRV_RSP_GENERAL_ERROR on any
 * out-of-range or null argument.
 */
hseSrvResponse_t HSE_SendRust(uint8_t u8MuInstance, uint8_t u8MuChannel,
                               hseTxOptions_t txOptions,
                               hseSrvDescriptor_t *pHseSrvDesc);

/**
 * @brief  Rust wrapper for HSE_GetFreeChannel with bounds checking.
 *
 * Returns HSE_INVALID_CHANNEL (0xFF) if @p u8MuInstance is out of range,
 * otherwise forwards to HSE_GetFreeChannel.
 */
uint8_t HSE_GetFreeChannelRust(uint8_t u8MuInstance);

/**
 * @brief  Rust wrapper for HSE_ReceiveInterruptHandler with bounds checking.
 *
 * Does nothing if @p u8MuInstance \u2265 HSE_NUM_OF_MU_INSTANCES, preventing an
 * out-of-bounds access into the ISR callback arrays.
 */
void HSE_ReceiveInterruptHandlerRust(uint8_t u8MuInstance);

/**
 * @brief  Rust wrapper for HSE_GeneralPurposeInterruptHandler with bounds checking.
 *
 * Does nothing if @p u8MuInstance \u2265 HSE_NUM_OF_MU_INSTANCES, preventing an
 * out-of-bounds access into the notification callback array.
 */
void HSE_GeneralPurposeInterruptHandlerRust(uint8_t u8MuInstance);

/**
 * @brief  Rust wrapper for HSE_RegisterGeneralPurposeCallback with bounds checking.
 *
 * Pass @p u8MuInstance == 0xFF to register on all MU instances (same sentinel
 * as the C implementation).  Any other out-of-range value is silently ignored,
 * preventing an out-of-bounds write to the callback array.
 */
void HSE_RegisterGeneralPurposeCallbackRust(uint8_t u8MuInstance,
                                             uint32_t u32ErrorsMask,
                                             pfGeneralPurposeCallback_t pfGeneralPurposeCallback);

/**
 * @brief  Loads two AES-128 keys into HSE RAM key slots for the secure channel.
 *
 * Must be called once before any `secure_send_to_ap_encrypted` or
 * `secure_recv_from_ap_encrypted` call.  Both pointers must address exactly
 * 16 valid bytes.
 *
 * Key-slot assignment:
 *   - RAM group-1 slot-0: AES-CBC-128 cipher key (ENCRYPT | DECRYPT, CBC only)
 *   - RAM group-1 slot-1: AES-CMAC-128 auth  key (SIGN | VERIFY, any mode)
 *
 * @param  cipher_key  16-byte AES-CBC-128 encryption key (must not be NULL).
 * @param  mac_key     16-byte AES-CMAC-128 authentication key (must not be NULL).
 * @return             true on success, false if HSE key-import fails or a
 *                     pointer is NULL.
 */
/**
 * @brief  Initialises the secure channel using the constant SHE example keys.
 *
 * Convenience wrapper: loads the same AES-128 key bytes used by
 * `SheLoadNVMKey_CBC` and `SheLoadNVMKey_CMAC` in `hse_host_wrappers.c`
 * into the HSE RAM slots, then sets up the secure-channel handles.
 *
 * Call this once after the SHE key catalog is configured and before any
 * `secure_send_to_ap_encrypted` or `secure_recv_from_ap_encrypted` call.
 *
 * @return  true on success, false if HSE key-import fails.
 */
bool secure_comm_init_she_keys(void);

bool secure_comm_init(const uint8_t *cipher_key, const uint8_t *mac_key);

/**
 * @brief  Encrypt and transmit @p len bytes of plaintext over LPUART.
 *
 * Builds the wire frame:
 *   IV(16) || AES-CBC(plaintext) || CMAC-128(IV || ciphertext)
 *
 * @p len must be a non-zero multiple of 16 and \u2264 224.  The caller is
 * responsible for padding the plaintext to a 16-byte boundary before calling
 * this function.
 *
 * @param  data  Plaintext source buffer (must not be NULL).
 * @param  len   Number of plaintext bytes (multiple of 16, \u2264 224).
 * @return       true on success, false on any crypto or transport error.
 */
bool secure_send_to_ap_encrypted(const uint8_t *data, uint32_t len);

/**
 * @brief  Receive and decrypt a secure frame from the application processor.
 *
 * Receives exactly (@p expected_ct_len + 32) bytes over LPUART, verifies the
 * AES-CMAC-128 tag, and \u2014 only if the tag is valid \u2014 decrypts the ciphertext
 * into @p buf.
 *
 * @p expected_ct_len must match the plaintext length used by the sender
 * (cipher+plain lengths are equal \u2014 no padding is added by this layer).
 *
 * @param  buf              Output buffer for the decrypted plaintext
 *                          (must be \u2265 @p expected_ct_len bytes, must not be NULL).
 * @param  expected_ct_len  Expected ciphertext length in bytes
 *                          (multiple of 16, \u2264 224).
 * @param  out_len          Receives the actual plaintext byte count on success
 *                          (must not be NULL).
 * @return                  true on success, false on transport, CMAC mismatch,
 *                          or HSE error.
 */
bool secure_recv_from_ap_encrypted(uint8_t *buf, uint32_t expected_ct_len,
                                   uint32_t *out_len);

/* -------------------------------------------------------------------------
 * HMAC services (pure-Rust MMIO driver, hse.rs)
 * ------------------------------------------------------------------------- */

/**
 * @brief  Hash algorithm selector for HMAC operations.
 *
 * Values match `hseHashAlgo_t` from `hse_common_types.h`.  SHA-3 and
 * Miyaguchi–Preneel are not supported by the HSE HMAC service.
 */
typedef enum {
    HSE_HMAC_ALGO_SHA1     = 2, /**< SHA-1,      20-byte tag */
    HSE_HMAC_ALGO_SHA2_224 = 3, /**< SHA-224,    28-byte tag */
    HSE_HMAC_ALGO_SHA2_256 = 4, /**< SHA-256,    32-byte tag (recommended) */
    HSE_HMAC_ALGO_SHA2_384 = 5, /**< SHA-384,    48-byte tag */
    HSE_HMAC_ALGO_SHA2_512 = 6  /**< SHA-512,    64-byte tag */
} HseHmacAlgo;

/**
 * @brief  Import a plain HMAC key into an HSE key slot.
 *
 * @param  target_key_handle  Destination slot (use @p GET_KEY_HANDLE()).
 * @param  key_flags          Usage flags, e.g.
 *                            `HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY`.
 * @param  key_bit_len        Key length in bits (e.g.\ 256).
 * @param  key_bytes          Raw key material (must not be NULL).
 * @param  key_len            Byte length of @p key_bytes
 *                            (must equal @p key_bit_len / 8).
 * @return                    true on success.
 */
bool Hse_LoadHmacKey(uint32_t target_key_handle, uint16_t key_flags,
                     uint16_t key_bit_len, const uint8_t *key_bytes,
                     uint32_t key_len);

/**
 * @brief  Compute an HMAC tag over @p input.
 *
 * On entry @p *tag_len must hold the capacity of @p tag (must be
 * ≥ the hash output length for the chosen algorithm).  On success
 * @p *tag_len is updated to the actual number of bytes written.
 *
 * @param  key_handle   HSE key handle with the SIGN flag.
 * @param  hash_algo    Underlying hash (see @ref HseHmacAlgo).
 * @param  input        Input data (may be NULL if @p input_len is 0).
 * @param  input_len    Byte length of @p input.
 * @param  tag          Output buffer (must not be NULL).
 * @param  tag_len      [in]  Buffer capacity in bytes.
 *                      [out] Actual tag length written on success.
 * @return              true on success.
 */
bool Hse_HmacGenerate(uint32_t key_handle, uint8_t hash_algo,
                      const uint8_t *input, uint32_t input_len,
                      uint8_t *tag, uint32_t *tag_len);

/**
 * @brief  Verify an HMAC tag over @p input.
 *
 * @param  key_handle   HSE key handle with the VERIFY flag.
 * @param  hash_algo    Must match the algorithm used to generate the tag.
 * @param  input        Input data.
 * @param  input_len    Byte length of @p input.
 * @param  tag          Reference tag to compare against (must not be NULL).
 * @param  tag_len      Byte length of @p tag  (must be in [8, hash_len]).
 * @return              true if the tag matches, false on mismatch or error.
 */
bool Hse_HmacVerify(uint32_t key_handle, uint8_t hash_algo,
                    const uint8_t *input, uint32_t input_len,
                    const uint8_t *tag, uint32_t tag_len);

#ifdef __cplusplus
}
#endif

#endif /* HSE_RUST_H */
