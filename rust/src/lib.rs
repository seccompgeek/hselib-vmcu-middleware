//! hselib_rust — Rust component of the S32K3 HSE library.
//!
//! # Cross-language call conventions
//!
//! ## C calling Rust (`include/hse_rust.h`)
//! Every `pub extern "C"` function annotated with `#[no_mangle]` in this crate
//! is callable from C.  The corresponding C declaration lives in
//! `include/hse_rust.h`.
//!
//! ## Rust calling C (`src/ffi.rs`)
//! `extern "C"` blocks in `ffi.rs` declare C function signatures.  The symbols
//! resolve at final application link time from the C objects that are already
//! merged into the same `libhselib.a` archive — no extra CMake plumbing is
//! needed.
//!
//! # Adding Rust → C calls
//! 1. Declare the C function in `ffi.rs` inside an `extern "C"` block.
//! 2. Write a safe Rust wrapper in this file (or a sub-module) that validates
//!    invariants before forwarding to the raw FFI call.
//! 3. If the function should also be callable from C, add `#[no_mangle]` and
//!    `pub extern "C"` to the wrapper and a declaration in `include/hse_rust.h`.
//!
//! Target: thumbv7em-none-eabihf  (Cortex-M7, hard-float, bare-metal)

#![no_std]

use panic_halt as _;

/// FFI declarations: `extern "C"` blocks for C functions callable from Rust.
pub mod ffi;

/// Rust reimplementation of `hse_host.c` with Rust-owned globals.
pub mod hse;

/// Safe communication-middleware layer: MU channel management and HSE service dispatch.
pub mod communication_middleware;

use ffi::{HseRngClass, HseSrvResponse};

// =============================================================================
// C → Rust exports
// Functions below are callable from C via `include/hse_rust.h`.
// =============================================================================

/// Returns the bitwise NOT of `value`.
/// Declared in C as: `uint32_t hse_rust_example(uint32_t value);`
#[no_mangle]
pub extern "C" fn hse_rust_example(value: u32) -> u32 {
    !value
}

/// Fills `buf` with `len` bytes of DRG4-class random data sourced from the HSE
/// RNG (delegates to the C `GetRngDRG4Num` implementation).
///
/// Returns `true` on success, `false` on any HSE error.
///
/// Declared in C as:
///   `bool hse_rust_fill_random_drg4(uint8_t *buf, uint32_t len);`
#[no_mangle]
pub extern "C" fn hse_rust_fill_random_drg4(buf: *mut u8, len: u32) -> bool {
    if buf.is_null() || len == 0 {
        return false;
    }
    // SAFETY: buf points to a caller-owned buffer of at least `len` bytes;
    // the caller guarantees this by the C contract.  GetRngDRG4Num does not
    // retain the pointer after returning.
    let resp: HseSrvResponse = unsafe { ffi::GetRngDRG4Num(buf, len) };
    resp.is_ok()
}

// =============================================================================
// Safe Rust → C wrappers (internal use)
// These wrap the raw FFI calls in safe Rust APIs used by other Rust modules.
// =============================================================================

/// Fills `buf` with random bytes using the requested `rng_class`.
///
/// Returns `Ok(())` on success or `Err(HseSrvResponse)` on failure.
pub fn get_rng_bytes(buf: &mut [u8], rng_class: HseRngClass)
    -> Result<(), HseSrvResponse>
{
    let resp = unsafe {
        ffi::GetRngNum(buf.as_mut_ptr(), buf.len() as u32, rng_class)
    };
    if resp.is_ok() { Ok(()) } else { Err(resp) }
}

/// Copies `src` into `dst` with byte order reversed.
/// Panics (via `panic-halt`, triggering a hard-fault) if lengths differ.
pub fn reverse_mem_copy(dst: &mut [u8], src: &[u8]) {
    assert_eq!(dst.len(), src.len());
    unsafe { ffi::ReverseMemCpy(dst.as_mut_ptr(), src.as_ptr(), src.len() as u32) }
}

// =============================================================================
// HMAC C → Rust exports
// =============================================================================

/// Import a plain HMAC key into an HSE RAM key slot.
///
/// `key_flags` should be `HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY` (0x000C)
/// for a bidirectional key.  `key_bit_len` is the key length in bits
/// (e.g. 256 for a 32-byte key).  `key_bytes` must point to `key_len`
/// valid bytes.
///
/// Returns `true` on success.
///
/// Declared in C as:
///   `bool Hse_LoadHmacKey(uint32_t target_key_handle, uint16_t key_flags,
///                         uint16_t key_bit_len, const uint8_t *key_bytes,
///                         uint32_t key_len);`
#[no_mangle]
pub unsafe extern "C" fn Hse_LoadHmacKey(
    target_key_handle: u32,
    key_flags: u16,
    key_bit_len: u16,
    key_bytes: *const u8,
    key_len: u32,
) -> bool {
    if key_bytes.is_null() || key_len == 0 {
        return false;
    }
    let bytes = core::slice::from_raw_parts(key_bytes, key_len as usize);
    hse::import_plain_hmac_key(target_key_handle, key_flags, key_bit_len, bytes).is_ok()
}

/// Compute an HMAC tag over `input` and write it to `tag`.
///
/// `hash_algo` must be one of the `HSE_HMAC_ALGO_*` values from
/// `hse_rust.h`.  On entry `*tag_len` must hold the capacity of the `tag`
/// buffer (must be ≥ the hash output length for the selected algorithm);
/// on success `*tag_len` is updated to the actual number of bytes written.
///
/// Returns `true` on success.
///
/// Declared in C as:
///   `bool Hse_HmacGenerate(uint32_t key_handle, uint8_t hash_algo,
///                          const uint8_t *input, uint32_t input_len,
///                          uint8_t *tag, uint32_t *tag_len);`
#[no_mangle]
pub unsafe extern "C" fn Hse_HmacGenerate(
    key_handle: u32,
    hash_algo: u8,
    input: *const u8,
    input_len: u32,
    tag: *mut u8,
    tag_len: *mut u32,
) -> bool {
    if input.is_null() || tag.is_null() || tag_len.is_null() {
        return false;
    }
    let algo = match hse::HseHashAlgo::from_u8(hash_algo) {
        Some(a) => a,
        None => return false,
    };
    let cap = *tag_len as usize;
    if cap < algo.tag_len() {
        return false;
    }
    let input_slice = core::slice::from_raw_parts(input, input_len as usize);
    let tag_slice = core::slice::from_raw_parts_mut(tag, cap);
    match hse::hmac_generate(key_handle, algo, input_slice, tag_slice) {
        Ok(()) => {
            *tag_len = algo.tag_len() as u32;
            true
        }
        Err(_) => false,
    }
}

/// Verify an HMAC tag over `input`.
///
/// `hash_algo` must match the algorithm used to generate the tag.
/// `tag_len` is the byte length of the reference tag and must be in
/// `[8, hash_output_len]`.
///
/// Returns `true` if the tag matches, `false` on mismatch or any error.
///
/// Declared in C as:
///   `bool Hse_HmacVerify(uint32_t key_handle, uint8_t hash_algo,
///                        const uint8_t *input, uint32_t input_len,
///                        const uint8_t *tag, uint32_t tag_len);`
#[no_mangle]
pub unsafe extern "C" fn Hse_HmacVerify(
    key_handle: u32,
    hash_algo: u8,
    input: *const u8,
    input_len: u32,
    tag: *const u8,
    tag_len: u32,
) -> bool {
    if input.is_null() || tag.is_null() || tag_len == 0 {
        return false;
    }
    let algo = match hse::HseHashAlgo::from_u8(hash_algo) {
        Some(a) => a,
        None => return false,
    };
    let input_slice = core::slice::from_raw_parts(input, input_len as usize);
    let tag_slice = core::slice::from_raw_parts(tag, tag_len as usize);
    hse::hmac_verify(key_handle, algo, input_slice, tag_slice).is_ok()
}
