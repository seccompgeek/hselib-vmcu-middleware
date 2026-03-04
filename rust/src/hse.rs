//! `hse` — Pure Rust MMIO driver for a subset of HSE cryptographic services.
//!
//! This module communicates directly with the HSE hardware via the MU0
//! (Message Unit 0) peripheral registers on the S32K344.  No FFI calls to
//! the existing C host-wrapper library are made; the module does its own
//! register-level I/O using `read_volatile` / `write_volatile`.
//!
//! # Descriptor layout
//!
//! Every HSE call shares the same 256-byte, 4-byte-aligned descriptor format:
//!
//! | Bytes    | Field                    |
//! |----------|--------------------------|
//! | `[0..4]` | `srvId` (u32, LE)        |
//! | `[4..8]` | `srvMetaData` (u32) = 0  |
//! | `[8..]`  | service-specific struct  |
//!
//! Field offsets for each service are documented next to the [`Desc`] fill
//! code inside each public function, and are derived directly from the C
//! header pack definitions in `include/`.
//!
//! # Safety
//!
//! Every public function in this module is a **safe** Rust function.
//! `unsafe` blocks are used only for:
//! * Volatile MMIO reads/writes at known-good S32K344 MU0 peripheral
//!   addresses.
//! * Deriving 32-bit physical addresses from Rust references via
//!   `core::ptr::addr_of!` / `as u32`.  All referenced data outlives the
//!   fully synchronous HSE call because it is declared in the calling stack
//!   frame.
//!
//! HSE must be fully initialised (key-catalog formatted, etc.) before any
//! function in this module is called.

// =============================================================================
// MU0 register addresses  (S32K344, MU0 base 0x4038_C000)
// Offsets from hse_mu.h / hse_mu.c
// =============================================================================

const MU0_BASE: u32 = 0x4038_C000;

/// Flag Status Register — bits [15:0] per-channel busy, bits [31:16] HSE status.
const MU_FSR: u32 = MU0_BASE + 0x104;
/// Transmit Status Register — bit[ch] = 1 means TX register is empty (ready to send).
const MU_TSR: u32 = MU0_BASE + 0x124;
/// Receive Status Register — bit[ch] = 1 means a response is waiting in RR[ch].
const MU_RSR: u32 = MU0_BASE + 0x12C;

/// Transmit Register for channel 1 — write descriptor address here to trigger HSE.
/// TR[n] = MU0_BASE + 0x200 + n×4
const MU_TR1: u32 = MU0_BASE + 0x200 + 4;
/// Receive Register for channel 1 — read `hseSrvResponse_t` from here.
/// RR[n] = MU0_BASE + 0x280 + n×4
const MU_RR1: u32 = MU0_BASE + 0x280 + 4;

/// All C host-wrappers use MU0 channel 1 for ordinary service requests.
const MU_CHANNEL: u32 = 1;
const CHANNEL_MASK: u32 = 1u32 << MU_CHANNEL;

// =============================================================================
// HSE service response codes  (hse_srv_responses.h)
// =============================================================================

const HSE_SRV_RSP_OK: u32 = 0x55A5_AA33;
const HSE_SRV_RSP_VERIFY_FAILED: u32 = 0x55A5_A164;

// =============================================================================
// HSE service IDs  (hse_interface.h, HSE_SRV_VER_0 = 0x0000_0000)
// =============================================================================

const SRV_ID_SYM_CIPHER: u32 = 0x00A5_0203;
const SRV_ID_MAC: u32 = 0x00A5_0201;
const SRV_ID_GET_RANDOM: u32 = 0x0000_0300;
const SRV_ID_IMPORT_KEY: u32 = 0x0000_0104;

// =============================================================================
// Enum / flag constants  (hse_common_types.h, hse_keymgmt_common_types.h)
// =============================================================================

// hseAccessMode_t
const ACCESS_MODE_ONE_PASS: u8 = 0;

// hseCipherAlgo_t
const CIPHER_ALGO_AES: u8 = 0x10;

// hseCipherBlockMode_t
const CIPHER_BLOCK_MODE_CBC: u8 = 2;

// hseCipherDir_t
const CIPHER_DIR_DECRYPT: u8 = 0;
const CIPHER_DIR_ENCRYPT: u8 = 1;

// hseAuthDir_t
const AUTH_DIR_VERIFY: u8 = 0;
const AUTH_DIR_GENERATE: u8 = 1;

// hseMacAlgo_t
const MAC_ALGO_CMAC: u8 = 0x11;
const MAC_ALGO_HMAC: u8 = 0x20;

// hseHashAlgo_t
const HASH_ALGO_SHA_1: u8 = 2;
const HASH_ALGO_SHA2_224: u8 = 3;
const HASH_ALGO_SHA2_256: u8 = 4;
const HASH_ALGO_SHA2_384: u8 = 5;
const HASH_ALGO_SHA2_512: u8 = 6;

// hseRngClass_t
const RNG_CLASS_DRG4: u8 = 1;

// hseKeyType_t
const KEY_TYPE_AES: u8 = 0x12;
const KEY_TYPE_HMAC: u8 = 0x20;

// Sentinel value meaning "no key" for cipher/auth wrapper handles.
const HSE_INVALID_KEY_HANDLE: u32 = 0xFFFF_FFFF;

// =============================================================================
// Public types
// =============================================================================

/// Errors returned by HSE service calls.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum HseError {
    /// CMAC / signature verification failed (HSE_SRV_RSP_VERIFY_FAILED).
    VerifyFailed,
    /// HSE returned a non-OK status code or the MU channel timed out.
    /// The raw response value is preserved for diagnostics; `0` signals a
    /// local timeout rather than an HSE error.
    Raw(u32),
}

/// Hash algorithms supported for HMAC (`hseHashAlgo_t`).
///
/// SHA3 and Miyaguchi-Preneel are **not** supported by the HSE HMAC service.
/// The numeric value of each variant matches the `HSE_HASH_ALGO_*` constant
/// in `hse_common_types.h` and can be passed directly to
/// [`hmac_generate`] / [`hmac_verify`] / [`import_plain_hmac_key`].
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
#[repr(u8)]
pub enum HseHashAlgo {
    /// SHA-1 — 20-byte output.  Avoid in new designs; provided for SHE/legacy
    /// compatibility.
    Sha1     = HASH_ALGO_SHA_1,
    /// SHA-224 — 28-byte output.
    Sha2_224 = HASH_ALGO_SHA2_224,
    /// SHA-256 — 32-byte output.  Recommended for most uses.
    Sha2_256 = HASH_ALGO_SHA2_256,
    /// SHA-384 — 48-byte output.
    Sha2_384 = HASH_ALGO_SHA2_384,
    /// SHA-512 — 64-byte output.
    Sha2_512 = HASH_ALGO_SHA2_512,
}

impl HseHashAlgo {
    /// HMAC output (tag) length in bytes for this hash.
    pub const fn tag_len(self) -> usize {
        match self {
            HseHashAlgo::Sha1     => 20,
            HseHashAlgo::Sha2_224 => 28,
            HseHashAlgo::Sha2_256 => 32,
            HseHashAlgo::Sha2_384 => 48,
            HseHashAlgo::Sha2_512 => 64,
        }
    }

    /// Construct from a raw `hseHashAlgo_t` byte (as returned/expected by HSE).
    /// Returns `None` for values not in the supported set.
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            HASH_ALGO_SHA_1     => Some(Self::Sha1),
            HASH_ALGO_SHA2_224  => Some(Self::Sha2_224),
            HASH_ALGO_SHA2_256  => Some(Self::Sha2_256),
            HASH_ALGO_SHA2_384  => Some(Self::Sha2_384),
            HASH_ALGO_SHA2_512  => Some(Self::Sha2_512),
            _ => None,
        }
    }
}

/// AES key usage flags (`hseKeyFlags_t` from `hse_keymgmt_common_types.h`).
///
/// Pass one of these constants — or an `|`-combination — as the `key_flags`
/// argument to [`import_plain_sym_key`] or [`import_plain_hmac_key`].
pub mod key_flags {
    /// Allow AES encryption (HSE_KF_USAGE_ENCRYPT = bit 0).
    pub const ENCRYPT: u16 = 0x0001;
    /// Allow AES decryption (HSE_KF_USAGE_DECRYPT = bit 1).
    pub const DECRYPT: u16 = 0x0002;
    /// Allow MAC / CMAC generation (HSE_KF_USAGE_SIGN = bit 2).
    pub const SIGN: u16 = 0x0004;
    /// Allow MAC / CMAC verification (HSE_KF_USAGE_VERIFY = bit 3).
    pub const VERIFY: u16 = 0x0008;
    /// Convenience: encryption + decryption.
    pub const ENCRYPT_DECRYPT: u16 = ENCRYPT | DECRYPT;
    /// Convenience: MAC generation + verification.
    pub const SIGN_VERIFY: u16 = SIGN | VERIFY;
}

// =============================================================================
// Private: bare-metal MU register helpers
// =============================================================================

#[inline]
fn mu_r(reg: u32) -> u32 {
    // SAFETY: `reg` is always one of the known-good MU0 peripheral register
    // addresses defined as constants in this module.
    unsafe { core::ptr::read_volatile(reg as *const u32) }
}

#[inline]
fn mu_w(reg: u32, val: u32) {
    // SAFETY: same as mu_r.
    unsafe { core::ptr::write_volatile(reg as *mut u32, val) }
}

/// Returns `true` when MU0 channel 1 is busy according to the hardware.
///
/// A channel is busy whenever any of the following hold (IS_SERVICE_CHANNEL_BUSY
/// macro from hse_mu.c):
/// * FSR bit[ch] set   → HSE is still processing the previous request.
/// * TSR bit[ch] clear → TX register has not been acknowledged yet.
/// * RSR bit[ch] set   → a response is pending and has not been read.
#[inline]
fn channel_busy() -> bool {
    (mu_r(MU_FSR) & CHANNEL_MASK != 0)
        || (mu_r(MU_TSR) & CHANNEL_MASK == 0)
        || (mu_r(MU_RSR) & CHANNEL_MASK != 0)
}

// =============================================================================
// Private: service descriptor buffer
// =============================================================================

/// 256-byte, 4-byte-aligned buffer holding one HSE service descriptor.
///
/// The entire buffer is zero-initialised on construction; only the fields
/// needed for each service are written explicitly.
#[repr(C, align(4))]
struct Desc([u8; 256]);

impl Desc {
    fn new(srv_id: u32) -> Self {
        let mut d = Desc([0u8; 256]);
        d.w32(0, srv_id); // srvId at [0..4]; srvMetaData at [4..8] stays 0
        d
    }

    #[inline(always)]
    fn w8(&mut self, off: usize, v: u8) {
        self.0[off] = v;
    }

    #[inline(always)]
    fn w16(&mut self, off: usize, v: u16) {
        self.0[off] = v as u8;
        self.0[off + 1] = (v >> 8) as u8;
    }

    #[inline(always)]
    fn w32(&mut self, off: usize, v: u32) {
        self.0[off] = v as u8;
        self.0[off + 1] = (v >> 8) as u8;
        self.0[off + 2] = (v >> 16) as u8;
        self.0[off + 3] = (v >> 24) as u8;
    }
}

// =============================================================================
// Private: synchronous MU send
// =============================================================================

/// Send `desc` to HSE over MU0 channel 1 and return the service response.
///
/// The function spins until the channel is free, writes the descriptor's
/// physical address to TR[1] to trigger processing, then blocks until the
/// response register is filled.  A `u32::MAX` spin-count is used as a
/// backstop; timeout yields `Err(HseError::Raw(0))`.
fn hse_send(desc: &Desc) -> Result<(), HseError> {
    const TIMEOUT: u32 = 0xFFFF_FFFF;

    // ── 1. Wait for channel 1 to be idle ─────────────────────────────────
    let mut ticks = TIMEOUT;
    while ticks > 0 && channel_busy() {
        ticks -= 1;
    }
    if ticks == 0 {
        return Err(HseError::Raw(0));
    }

    // ── 2. Compiler fence: ensure all descriptor writes reach memory before
    //       the MMIO trigger write that follows.
    core::sync::atomic::compiler_fence(core::sync::atomic::Ordering::Release);

    // ── 3. Write descriptor physical address to TR[1] — starts HSE ───────
    let addr = desc as *const Desc as u32;
    mu_w(MU_TR1, addr);

    // ── 4. Poll RSR until the channel-1 response bit is set ──────────────
    let mut ticks = TIMEOUT;
    while ticks > 0 && (mu_r(MU_RSR) & CHANNEL_MASK == 0) {
        ticks -= 1;
    }
    if ticks == 0 {
        return Err(HseError::Raw(0));
    }

    // ── 5. Read the service response from RR[1] ───────────────────────────
    let resp = mu_r(MU_RR1);

    // ── 6. Wait for FSR channel-1 bit to clear (HSE fully done) ──────────
    // This mirrors the C: `while(MU_IS_BIT_SET(HSE_MU_READ_FLAG_STATUS_REGISTER(...), ch)) {}`
    // Timeout here is non-fatal; the response has already been captured.
    let mut ticks = TIMEOUT;
    while ticks > 0 && (mu_r(MU_FSR) & CHANNEL_MASK != 0) {
        ticks -= 1;
    }

    match resp {
        HSE_SRV_RSP_OK => Ok(()),
        HSE_SRV_RSP_VERIFY_FAILED => Err(HseError::VerifyFailed),
        other => Err(HseError::Raw(other)),
    }
}

// =============================================================================
// Public API
// =============================================================================

// ── AES-CBC encrypt ───────────────────────────────────────────────────────────

/// Encrypt `input` in-place style with AES-CBC into `output`.
///
/// Fills the `hseSymCipherSrv_t` descriptor fields (bytes 8–36 of the
/// service descriptor):
///
/// | Desc offset | Field           | Value                    |
/// |-------------|-----------------|--------------------------|
/// | `[8]`       | accessMode      | ONE_PASS (0)             |
/// | `[9]`       | streamId        | 0                        |
/// | `[10]`      | cipherAlgo      | AES (0x10)               |
/// | `[11]`      | cipherBlockMode | CBC (2)                  |
/// | `[12]`      | cipherDir       | ENCRYPT (1)              |
/// | `[13]`      | sgtOption       | NONE (0)                 |
/// | `[16..20]`  | keyHandle       | `key_handle`             |
/// | `[20..24]`  | pIV             | address of `iv`          |
/// | `[24..28]`  | inputLength     | `input.len()` (bytes)    |
/// | `[28..32]`  | pInput          | address of `input`       |
/// | `[32..36]`  | pOutput         | address of `output`      |
///
/// # Parameters
/// * `key_handle` — HSE key handle for an AES key already in the key catalog.
/// * `iv` — 16-byte initialisation vector.
/// * `input` — plaintext; length must be a non-zero multiple of 16.
/// * `output` — ciphertext buffer; must be at least `input.len()` bytes long.
pub fn aes_cbc_encrypt(
    key_handle: u32,
    iv: &[u8; 16],
    input: &[u8],
    output: &mut [u8],
) -> Result<(), HseError> {
    let mut d = Desc::new(SRV_ID_SYM_CIPHER);
    d.w8(8, ACCESS_MODE_ONE_PASS);
    d.w8(9, 0); // streamId
    d.w8(10, CIPHER_ALGO_AES);
    d.w8(11, CIPHER_BLOCK_MODE_CBC);
    d.w8(12, CIPHER_DIR_ENCRYPT);
    d.w8(13, 0); // sgtOption = NONE; [14..15] reserved = 0
    d.w32(16, key_handle);
    d.w32(20, iv.as_ptr() as u32); // pIV
    d.w32(24, input.len() as u32); // inputLength
    d.w32(28, input.as_ptr() as u32); // pInput
    d.w32(32, output.as_mut_ptr() as u32); // pOutput
    hse_send(&d)
}

// ── AES-CBC decrypt ───────────────────────────────────────────────────────────

/// Decrypt `input` with AES-CBC into `output`.
///
/// Descriptor layout is identical to [`aes_cbc_encrypt`] except
/// `cipherDir = DECRYPT (0)` at byte `[12]`.
///
/// # Parameters
/// * `key_handle` — HSE key handle for an AES key already in the key catalog.
/// * `iv` — 16-byte initialisation vector.
/// * `input` — ciphertext; length must be a non-zero multiple of 16.
/// * `output` — plaintext buffer; must be at least `input.len()` bytes long.
pub fn aes_cbc_decrypt(
    key_handle: u32,
    iv: &[u8; 16],
    input: &[u8],
    output: &mut [u8],
) -> Result<(), HseError> {
    let mut d = Desc::new(SRV_ID_SYM_CIPHER);
    d.w8(8, ACCESS_MODE_ONE_PASS);
    d.w8(9, 0);
    d.w8(10, CIPHER_ALGO_AES);
    d.w8(11, CIPHER_BLOCK_MODE_CBC);
    d.w8(12, CIPHER_DIR_DECRYPT);
    d.w8(13, 0);
    d.w32(16, key_handle);
    d.w32(20, iv.as_ptr() as u32);
    d.w32(24, input.len() as u32);
    d.w32(28, input.as_ptr() as u32);
    d.w32(32, output.as_mut_ptr() as u32);
    hse_send(&d)
}

// ── AES-CMAC generate ─────────────────────────────────────────────────────────

/// Generate a 16-byte AES-CMAC tag over `input` and write it into `tag`.
///
/// Fills the `hseMacSrv_t` descriptor fields (bytes 8–44):
///
/// | Desc offset | Field                         | Value             |
/// |-------------|-------------------------------|-------------------|
/// | `[8]`       | accessMode                    | ONE_PASS (0)      |
/// | `[9]`       | streamId                      | 0                 |
/// | `[10]`      | authDir                       | GENERATE (1)      |
/// | `[11]`      | sgtOption                     | 0                 |
/// | `[12]`      | macScheme.macAlgo             | CMAC (0x11)       |
/// | `[13..16]`  | macScheme.reserved            | 0                 |
/// | `[16]`      | macScheme.sch.cmac.cipherAlgo | AES (0x10)        |
/// | `[17..24]`  | cmac.reserved / union pad     | 0                 |
/// | `[24..28]`  | keyHandle                     | `key_handle`      |
/// | `[28..32]`  | inputLength                   | `input.len()`     |
/// | `[32..36]`  | pInput                        | address of input  |
/// | `[36..40]`  | pTagLength                    | address of local  |
/// | `[40..44]`  | pTag                          | address of `tag`  |
///
/// `hseMacScheme_t` layout:  `macAlgo(1) + reserved[3] + union{cmac/hmac/gmac/xcbc}`
/// where `gmac` is the largest member at 8 bytes → total 12 bytes.
/// Key handle follows at struct offset 16, i.e. descriptor byte 24.
pub fn aes_cmac_generate(
    key_handle: u32,
    input: &[u8],
    tag: &mut [u8; 16],
) -> Result<(), HseError> {
    // `tag_len` provides the pTagLength pointer.  HSE writes back the actual
    // generated tag length here; we initialise it to 16 (full CMAC length).
    let mut tag_len: u32 = 16;

    let mut d = Desc::new(SRV_ID_MAC);
    d.w8(8, ACCESS_MODE_ONE_PASS);
    d.w8(9, 0); // streamId
    d.w8(10, AUTH_DIR_GENERATE);
    d.w8(11, 0); // sgtOption
    // macScheme.macAlgo at [12]; .reserved[3] at [13,14,15] = 0
    d.w8(12, MAC_ALGO_CMAC);
    // macScheme.sch.cmac: cipherAlgo at [16], reserved[3] at [17,18,19] = 0
    // Remaining 4 bytes of the 8-byte gmac-sized union ([20..23]) = 0
    d.w8(16, CIPHER_ALGO_AES);
    d.w32(24, key_handle); // struct offset 16 + desc header 8 = desc[24]
    d.w32(28, input.len() as u32); // inputLength
    d.w32(32, input.as_ptr() as u32); // pInput
    // SAFETY: `tag_len` is a local variable whose address is valid for the
    // duration of this function.  HSE writes the actual tag length there.
    d.w32(36, core::ptr::addr_of_mut!(tag_len) as u32); // pTagLength
    d.w32(40, tag.as_mut_ptr() as u32); // pTag

    let r = hse_send(&d);

    // Volatile read ensures the compiler keeps `tag_len` on the stack
    // throughout `hse_send`, allowing HSE to write back the tag length.
    let _ = unsafe { core::ptr::read_volatile(core::ptr::addr_of!(tag_len)) };

    r
}

// ── AES-CMAC verify ───────────────────────────────────────────────────────────

/// Verify a 16-byte AES-CMAC `tag` over `input`.
///
/// Returns `Ok(())` when the tag matches, [`HseError::VerifyFailed`]
/// (`HSE_SRV_RSP_VERIFY_FAILED`) when it does not.
///
/// Descriptor layout is identical to [`aes_cmac_generate`] except
/// `authDir = VERIFY (0)` at byte `[10]` and `pTag` points to the
/// known-good read-only `tag` slice.
pub fn aes_cmac_verify(
    key_handle: u32,
    input: &[u8],
    tag: &[u8; 16],
) -> Result<(), HseError> {
    // HSE reads the expected tag length from pTagLength.
    let tag_len: u32 = 16;

    let mut d = Desc::new(SRV_ID_MAC);
    d.w8(8, ACCESS_MODE_ONE_PASS);
    d.w8(9, 0);
    d.w8(10, AUTH_DIR_VERIFY);
    d.w8(11, 0);
    d.w8(12, MAC_ALGO_CMAC);
    d.w8(16, CIPHER_ALGO_AES);
    d.w32(24, key_handle);
    d.w32(28, input.len() as u32);
    d.w32(32, input.as_ptr() as u32);
    // SAFETY: `tag_len` is a local variable valid for the full call duration.
    d.w32(36, core::ptr::addr_of!(tag_len) as u32); // pTagLength (HSE reads this)
    d.w32(40, tag.as_ptr() as u32); // pTag (HSE reads the expected tag here)
    hse_send(&d)
}

// ── RNG DRG4 ──────────────────────────────────────────────────────────────────

/// Fill `output` with cryptographically random bytes from the HSE DRG4 DRBG.
///
/// Fills the `hseGetRandomNumSrv_t` descriptor fields (bytes 8–20):
///
/// | Desc offset | Field            | Value               |
/// |-------------|------------------|---------------------|
/// | `[8]`       | rngClass         | DRG4 (1)            |
/// | `[9..12]`   | reserved         | 0                   |
/// | `[12..16]`  | randomNumLength  | `output.len()`      |
/// | `[16..20]`  | pRandomNum       | address of `output` |
pub fn get_rng_drg4(output: &mut [u8]) -> Result<(), HseError> {
    let mut d = Desc::new(SRV_ID_GET_RANDOM);
    d.w8(8, RNG_CLASS_DRG4); // rngClass; reserved[3] at [9,10,11] = 0
    d.w32(12, output.len() as u32); // randomNumLength
    d.w32(16, output.as_mut_ptr() as u32); // pRandomNum
    hse_send(&d)
}

// ── Plain symmetric key import ────────────────────────────────────────────────

/// Import a plain (unencrypted, unauthenticated) AES symmetric key into HSE.
///
/// Mirrors `ImportPlainSymKeyReqMuChannel` from `hse_host_import_key.c`:
/// zeroes the descriptor, sets the target slot, and leaves all cipher/auth
/// wrapper fields at their "invalid" sentinels.
///
/// The 16-byte `hseKeyInfo_t` is stack-allocated adjacent to the descriptor
/// and its physical address is embedded in `pKeyInfo`.
///
/// `hseImportKeySrv_t` descriptor layout (bytes 8–103):
///
/// | Desc offset | Field                        | Value                 |
/// |-------------|------------------------------|-----------------------|
/// | `[8..12]`   | targetKeyHandle              | `target_key_handle`   |
/// | `[12..16]`  | pKeyInfo                     | address of `ki`       |
/// | `[16..20]`  | pKey\[0\]                    | 0 (RSA pub — unused)  |
/// | `[20..24]`  | pKey\[1\]                    | 0 (RSA exp — unused)  |
/// | `[24..28]`  | pKey\[2\]                    | address of `key_bytes`|
/// | `[28..30]`  | keyLen\[0\]                  | 0                     |
/// | `[30..32]`  | keyLen\[1\]                  | 0                     |
/// | `[32..34]`  | keyLen\[2\]                  | `key_bytes.len()`     |
/// | `[34..36]`  | reserved\[2\]                | 0                     |
/// | `[36..40]`  | cipher.cipherKeyHandle       | `0xFFFF_FFFF` (plain) |
/// | `[40..64]`  | cipher.cipherScheme (24 B)   | 0 (no encryption)     |
/// | `[64..66]`  | keyContainer.keyContainerLen | 0                     |
/// | `[66..68]`  | keyContainer.reserved        | 0                     |
/// | `[68..72]`  | keyContainer.pKeyContainer   | 0                     |
/// | `[72..76]`  | keyContainer.authKeyHandle   | `0xFFFF_FFFF` (none)  |
/// | `[76..88]`  | keyContainer.authScheme (12B)| 0 (no auth)           |
/// | `[88..92]`  | keyContainer.authLen\[2\]    | 0                     |
/// | `[92..100]` | keyContainer.pAuth\[2\]      | 0                     |
/// | `[100..104]`| keyFormat                    | 0                     |
///
/// `hseKeyInfo_t` layout (16 bytes, `#pragma pack` / natural alignment):
///
/// | Offset | Field                | Value                    |
/// |--------|----------------------|--------------------------|
/// | `[0]`  | keyFlags (u16)       | `key_flags`              |
/// | `[2]`  | keyBitLen (u16)      | `key_bit_len`            |
/// | `[4]`  | keyCounter (u32)     | 0 (RAM key)              |
/// | `[8]`  | smrFlags (u32)       | 0                        |
/// | `[12]` | keyType (u8)         | AES (0x12)               |
/// | `[13]` | aesBlockModeMask (u8)| `aes_block_mode_mask`    |
/// | `[14]` | hseReserved\[2\]     | 0                        |
///
/// # Parameters
/// * `target_key_handle` — destination slot in the HSE key catalog.
/// * `key_flags` — usage flags; combine constants from [`key_flags`].
/// * `key_bit_len` — key length in bits (e.g. 128 for AES-128).
/// * `aes_block_mode_mask` — allowed AES block modes (0 = all modes).
///   Corresponds to `hseAesBlockModeMask_t` / `specific.aesBlockModeMask`.
/// * `key_bytes` — raw key material; byte length must equal `key_bit_len / 8`.
pub fn import_plain_sym_key(
    target_key_handle: u32,
    key_flags: u16,
    key_bit_len: u16,
    aes_block_mode_mask: u8,
    key_bytes: &[u8],
) -> Result<(), HseError> {
    // hseKeyInfo_t — 16 bytes, 4-byte aligned.  Must remain on the stack for
    // the entire duration of the synchronous HSE call.
    #[repr(C, align(4))]
    struct KeyInfo([u8; 16]);

    let mut ki = KeyInfo([0u8; 16]);
    // keyFlags  (u16 LE) at offset +0
    ki.0[0] = key_flags as u8;
    ki.0[1] = (key_flags >> 8) as u8;
    // keyBitLen (u16 LE) at offset +2
    ki.0[2] = key_bit_len as u8;
    ki.0[3] = (key_bit_len >> 8) as u8;
    // keyCounter (u32) at +4 = 0 for RAM keys  (already zero)
    // smrFlags   (u32) at +8 = 0               (already zero)
    // keyType    (u8)  at +12
    ki.0[12] = KEY_TYPE_AES;
    // specific.aesBlockModeMask (u8) at +13
    ki.0[13] = aes_block_mode_mask;
    // hseReserved[2] at +14,+15 = 0            (already zero)

    let mut d = Desc::new(SRV_ID_IMPORT_KEY);

    // hseImportKeySrv_t starts at descriptor byte 8
    d.w32(8, target_key_handle); // targetKeyHandle
    // SAFETY: `ki` is declared in this frame and outlives `hse_send`.
    d.w32(12, core::ptr::addr_of!(ki) as u32); // pKeyInfo
    d.w32(16, 0); // pKey[0] (RSA/ECC public key — not used for symmetric)
    d.w32(20, 0); // pKey[1] (RSA public exponent — not used)
    d.w32(24, key_bytes.as_ptr() as u32); // pKey[2] = raw symmetric key
    d.w16(28, 0); // keyLen[0]
    d.w16(30, 0); // keyLen[1]
    d.w16(32, key_bytes.len() as u16); // keyLen[2] (byte length of key)
    // reserved[2] at [34,35] = 0              (already zero)
    d.w32(36, HSE_INVALID_KEY_HANDLE); // cipher.cipherKeyHandle (plain import — no encryption)
    // cipher.cipherScheme [40..63] = 0        (already zero)
    // keyContainer.keyContainerLen [64..65] = 0 (already zero)
    // keyContainer.reserved [66..67] = 0      (already zero)
    // keyContainer.pKeyContainer [68..71] = 0 (already zero)
    d.w32(72, HSE_INVALID_KEY_HANDLE); // keyContainer.authKeyHandle (no auth tag)
    // keyContainer.authScheme [76..87] = 0    (already zero)
    // keyContainer.authLen[2] [88..91] = 0    (already zero)
    // keyContainer.pAuth[2] [92..99] = 0      (already zero)
    // keyFormat [100..103] = 0                (already zero)

    let r = hse_send(&d);

    // Volatile read forces the compiler to keep `ki` allocated on the stack
    // throughout `hse_send`, preventing the optimizer from reusing that
    // stack slot before the HSE call returns.
    let _ = unsafe { core::ptr::read_volatile(core::ptr::addr_of!(ki.0[0])) };

    r
}

// ── Plain HMAC key import ─────────────────────────────────────────────────────

/// Import a plain (unencrypted, unauthenticated) HMAC key into an HSE key slot.
///
/// Identical to [`import_plain_sym_key`] except:
/// * `keyType = HSE_KEY_TYPE_HMAC (0x20)` — informs HSE that the key slot
///   holds an HMAC key, which can only be used with [`hmac_generate`] /
///   [`hmac_verify`].  AES cipher operations on this handle will be rejected.
/// * The `specific.aesBlockModeMask` byte is set to 0 (unused for HMAC).
///
/// # Parameters
/// * `target_key_handle` — destination slot in the HSE key catalog.
/// * `key_flags` — usage flags; use [`key_flags::SIGN_VERIFY`] for a bidirectional
///   HMAC key, or [`key_flags::SIGN`] / [`key_flags::VERIFY`] for a one-way key.
/// * `key_bit_len` — key length in bits (e.g. 256 for a 32-byte HMAC key);
///   HSE supports HMAC keys from 128 to 4096 bits.
/// * `key_bytes` — raw key material; byte length must equal `key_bit_len / 8`.
pub fn import_plain_hmac_key(
    target_key_handle: u32,
    key_flags: u16,
    key_bit_len: u16,
    key_bytes: &[u8],
) -> Result<(), HseError> {
    #[repr(C, align(4))]
    struct KeyInfo([u8; 16]);

    let mut ki = KeyInfo([0u8; 16]);
    ki.0[0] = key_flags as u8;
    ki.0[1] = (key_flags >> 8) as u8;
    ki.0[2] = key_bit_len as u8;
    ki.0[3] = (key_bit_len >> 8) as u8;
    // keyCounter (u32) at +4 = 0 for RAM keys
    // smrFlags   (u32) at +8 = 0
    ki.0[12] = KEY_TYPE_HMAC;  // keyType
    // specific (u8) at +13 = 0  (aesBlockModeMask unused for HMAC)
    // hseReserved[2] at +14,+15 = 0

    let mut d = Desc::new(SRV_ID_IMPORT_KEY);
    d.w32(8,  target_key_handle);
    // SAFETY: `ki` outlives `hse_send`.
    d.w32(12, core::ptr::addr_of!(ki) as u32);
    d.w32(16, 0); // pKey[0] unused
    d.w32(20, 0); // pKey[1] unused
    d.w32(24, key_bytes.as_ptr() as u32); // pKey[2] = raw HMAC key
    d.w16(28, 0);                         // keyLen[0]
    d.w16(30, 0);                         // keyLen[1]
    d.w16(32, key_bytes.len() as u16);    // keyLen[2]
    d.w32(36, HSE_INVALID_KEY_HANDLE);    // cipher.cipherKeyHandle (plain)
    d.w32(72, HSE_INVALID_KEY_HANDLE);    // keyContainer.authKeyHandle (none)

    let r = hse_send(&d);
    let _ = unsafe { core::ptr::read_volatile(core::ptr::addr_of!(ki.0[0])) };
    r
}

// ── HMAC generate ─────────────────────────────────────────────────────────────

/// Generate an HMAC tag over `input` and write it into `tag`.
///
/// Uses the `hseMacSrv_t` descriptor (service ID `SRV_ID_MAC`) with
/// `macAlgo = HSE_MAC_ALGO_HMAC` and the `hashAlgo` sub-field set to
/// `hash_algo as u8`.  The descriptor layout is byte-for-byte compatible with
/// [`aes_cmac_generate`] — only bytes `[12]` and `[16]` differ:
///
/// | Desc offset | Field                          | Value                  |
/// |-------------|--------------------------------|------------------------|
/// | `[8]`       | accessMode                     | ONE_PASS (0)           |
/// | `[9]`       | streamId                       | 0                      |
/// | `[10]`      | authDir                        | GENERATE (1)           |
/// | `[11]`      | sgtOption                      | 0                      |
/// | `[12]`      | macScheme.macAlgo              | HMAC (0x20)            |
/// | `[13..16]`  | macScheme.reserved             | 0                      |
/// | `[16]`      | macScheme.sch.hmac.hashAlgo    | `hash_algo as u8`      |
/// | `[17..24]`  | hmac.reserved / union pad      | 0                      |
/// | `[24..28]`  | keyHandle                      | `key_handle`           |
/// | `[28..32]`  | inputLength                    | `input.len()`          |
/// | `[32..36]`  | pInput                         | address of `input`     |
/// | `[36..40]`  | pTagLength                     | address of `tag_len`   |
/// | `[40..44]`  | pTag                           | address of `tag`       |
///
/// # Parameters
/// * `key_handle` — handle for an HMAC key previously loaded with
///   [`import_plain_hmac_key`]; must have the `SIGN` flag set.
/// * `hash_algo` — selects the underlying hash; see [`HseHashAlgo`].
/// * `input` — data over which to compute the HMAC.
/// * `tag` — output buffer; must be at least `hash_algo.tag_len()` bytes long.
pub fn hmac_generate(
    key_handle: u32,
    hash_algo: HseHashAlgo,
    input: &[u8],
    tag: &mut [u8],
) -> Result<(), HseError> {
    let mut tag_len: u32 = tag.len() as u32;

    let mut d = Desc::new(SRV_ID_MAC);
    d.w8(8,  ACCESS_MODE_ONE_PASS);
    d.w8(9,  0); // streamId
    d.w8(10, AUTH_DIR_GENERATE);
    d.w8(11, 0); // sgtOption
    d.w8(12, MAC_ALGO_HMAC);
    // macScheme.reserved[3] at [13,14,15] = 0
    d.w8(16, hash_algo as u8); // macScheme.sch.hmac.hashAlgo
    // hmac.reserved[3] at [17,18,19] + union pad [20..23] = 0
    d.w32(24, key_handle);
    d.w32(28, input.len() as u32);
    d.w32(32, input.as_ptr() as u32);
    // SAFETY: `tag_len` is a local variable valid for the full call duration.
    d.w32(36, core::ptr::addr_of_mut!(tag_len) as u32);
    d.w32(40, tag.as_mut_ptr() as u32);

    let r = hse_send(&d);
    // Keep `tag_len` on the stack through `hse_send` so HSE can write the
    // actual generated length back.
    let _ = unsafe { core::ptr::read_volatile(core::ptr::addr_of!(tag_len)) };
    r
}

// ── HMAC verify ───────────────────────────────────────────────────────────────

/// Verify an HMAC `tag` over `input`.
///
/// Returns `Ok(())` if the tag matches, [`HseError::VerifyFailed`] if it does
/// not, or another [`HseError`] variant on HSE error.
///
/// Descriptor layout is identical to [`hmac_generate`] except
/// `authDir = VERIFY (0)` at byte `[10]` and `pTag` points to the
/// caller-supplied reference tag.
///
/// # Parameters
/// * `key_handle` — handle for an HMAC key with the `VERIFY` flag.
/// * `hash_algo` — must match the algorithm used to generate the tag.
/// * `input` — data over which to verify the HMAC.
/// * `tag` — reference tag; length must be in `[8, hash_algo.tag_len()]`.
pub fn hmac_verify(
    key_handle: u32,
    hash_algo: HseHashAlgo,
    input: &[u8],
    tag: &[u8],
) -> Result<(), HseError> {
    // HSE reads the tag length from pTagLength during VERIFY.
    let tag_len: u32 = tag.len() as u32;

    let mut d = Desc::new(SRV_ID_MAC);
    d.w8(8,  ACCESS_MODE_ONE_PASS);
    d.w8(9,  0);
    d.w8(10, AUTH_DIR_VERIFY);
    d.w8(11, 0);
    d.w8(12, MAC_ALGO_HMAC);
    d.w8(16, hash_algo as u8);
    d.w32(24, key_handle);
    d.w32(28, input.len() as u32);
    d.w32(32, input.as_ptr() as u32);
    // SAFETY: `tag_len` is a local variable valid for the full call duration.
    d.w32(36, core::ptr::addr_of!(tag_len) as u32);
    d.w32(40, tag.as_ptr() as u32);
    hse_send(&d)
}
