//! ffi.rs — Rust-side declarations for C functions in hselib.
//!
//! Every C function that Rust code needs to call must be declared here inside
//! an `extern "C"` block.  Rust will emit an undefined-symbol reference for
//! each one; it is resolved at final application link time from the C object
//! files that are merged into the same `libhselib.a` archive.
//!
//! # Adding new bindings
//!
//! 1. Find the C declaration in the relevant `include/hse_*.h` header.
//! 2. Translate types according to the mapping table at the top of this file.
//! 3. Use `*mut T` / `*const T` for pointer arguments.
//! 4. Mark the wrapper you write in lib.rs as `unsafe` if it can be called
//!    without additional invariants, or provide a safe wrapper that checks them.
//!
//! # Type-mapping reference
//!
//!  C type                    │  Rust type
//! ───────────────────────────┼────────────────────────────
//!  uint8_t / int8_t          │  u8  / i8
//!  uint16_t / int16_t        │  u16 / i16
//!  uint32_t / int32_t        │  u32 / i32
//!  uint64_t / int64_t        │  u64 / i64
//!  bool                      │  bool  (C99 _Bool)
//!  void *                    │  *mut core::ffi::c_void
//!  const void *              │  *const core::ffi::c_void
//!  hseSrvResponse_t          │  HseSrvResponse (u32 newtype below)
//!  hseRngClass_t             │  HseRngClass    (u32 newtype below)

// ── Transparent type aliases ──────────────────────────────────────────────────

/// `hseSrvResponse_t` — 32-bit service-response code returned by most HSE APIs.
///
/// Notable values (from hse_interface.h):
///   `0x00A5_5A00` → `HSE_SRV_RSP_OK`
///   `0x00A5_5A_A5` → `HSE_SRV_RSP_VERIFY_FAILED`
///   Any other value → error (see hse_interface.h for the full list)
#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct HseSrvResponse(pub u32);

impl HseSrvResponse {
    /// `HSE_SRV_RSP_OK` — service executed successfully.
    pub const OK: Self = Self(0x00A5_5A00);
    /// `HSE_SRV_RSP_GENERAL_ERROR` — unspecified internal error (from hse_srv_responses.h).
    pub const GENERAL_ERROR: Self = Self(0x33D6_D4F1);
    /// Returns `true` when the response indicates success.
    #[inline(always)]
    pub fn is_ok(self) -> bool {
        self == Self::OK
    }
}

/// `hseRngClass_t` — selects the DRBG class used by `GetRngNum`.
///
/// Values from hse_interface.h:
///   0 → HSE_RNG_CLASS_DRG3
///   1 → HSE_RNG_CLASS_DRG4
///   2 → HSE_RNG_CLASS_PTG3
#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct HseRngClass(pub u32);

impl HseRngClass {
    pub const DRG3: Self = Self(0);
    pub const DRG4: Self = Self(1);
    pub const PTG3: Self = Self(2);
}

// ── hse_host.h types ─────────────────────────────────────────────────────────

/// `hseTxOp_t` — selects synchronous or asynchronous transmission mode.
#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct HseTxOp(pub u32);

impl HseTxOp {
    /// `HSE_TX_SYNCHRONOUS`
    pub const SYNCHRONOUS: Self = Self(0);
    /// `HSE_TX_ASYNCHRONOUS`
    pub const ASYNCHRONOUS: Self = Self(1);
}

/// `pfAsyncCallback_t` — callback invoked on asynchronous TX completion.
///
/// C declaration: `void (*pfAsyncCallback_t)(hseSrvResponse_t status, void *pArg);`
pub type PfAsyncCallback =
    Option<unsafe extern "C" fn(status: HseSrvResponse, p_arg: *mut core::ffi::c_void)>;

/// `pfGeneralPurposeCallback_t` — callback for HSE general-purpose (notification) interrupts.
///
/// C declaration: `void (*pfGeneralPurposeCallback_t)(uint8_t u8MuInstance, uint32_t u32HseNotifEvents);`
pub type PfGeneralPurposeCallback =
    Option<unsafe extern "C" fn(u8_mu_instance: u8, u32_hse_notif_events: u32)>;

/// `hseTxOptions_t` — transmission options passed to `HSE_Send`.
///
/// Must stay `#[repr(C)]` to match the C struct layout exactly.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct HseTxOptions {
    /// Selects synchronous or asynchronous mode.
    pub tx_op: HseTxOp,
    /// Callback invoked on completion when `tx_op == ASYNCHRONOUS`; `None` otherwise.
    pub pf_async_callback: PfAsyncCallback,
    /// Opaque argument forwarded to `pf_async_callback`; may be null.
    pub p_callback_p_arg: *mut core::ffi::c_void,
}

/// Opaque handle for `hseSrvDescriptor_t`.
///
/// C code fills the descriptor before passing a pointer to `HSE_Send`.
/// Rust only ever holds a pointer to it, never constructs one itself.
#[repr(C)]
pub struct HseSrvDescriptor {
    _opaque: [u8; 0],
}

// ── extern "C" blocks ─────────────────────────────────────────────────────────

extern "C" {
    // ── hse_host_utils.h ─────────────────────────────────────────────────────

    /// Copies `len` bytes from `src` to `dst` in reversed byte order.
    ///
    /// C declaration:
    ///   `void ReverseMemCpy(uint8_t *dst, const uint8_t *src, uint32_t len);`
    pub fn ReverseMemCpy(dst: *mut u8, src: *const u8, len: u32);

    // ── hse_host_rng.h ───────────────────────────────────────────────────────

    /// Fills `rng_num[0..rng_num_size]` with random bytes using `rng_class`.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t GetRngNum(uint8_t *rngNum, uint32_t rngNumSize,
    ///                               hseRngClass_t rngClass);`
    pub fn GetRngNum(rng_num: *mut u8, rng_num_size: u32, rng_class: HseRngClass)
        -> HseSrvResponse;

    /// Fills `rng_num[0..rng_num_size]` with DRG3-class random bytes.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t GetRngDRG3Num(uint8_t *rngNum, uint32_t rngNumSize);`
    pub fn GetRngDRG3Num(rng_num: *mut u8, rng_num_size: u32) -> HseSrvResponse;

    /// Fills `rng_num[0..rng_num_size]` with DRG4-class random bytes.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t GetRngDRG4Num(uint8_t *rngNum, uint32_t rngNumSize);`
    pub fn GetRngDRG4Num(rng_num: *mut u8, rng_num_size: u32) -> HseSrvResponse;

    /// Fills `rng_num[0..rng_num_size]` with PTG3-class random bytes.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t GetRngPTG3Num(uint8_t *rngNum, uint32_t rngNumSize);`
    pub fn GetRngPTG3Num(rng_num: *mut u8, rng_num_size: u32) -> HseSrvResponse;
}

// ── Array dimension constants ─────────────────────────────────────────────────

/// Number of MU instances — must match `HSE_NUM_OF_MU_INSTANCES` in `hse_b_config.h`.
pub const NUM_MU_INSTANCES: usize = 2;

/// Number of channels per MU — must match `HSE_NUM_OF_CHANNELS_PER_MU` in `hse_b_config.h`.
pub const NUM_CHANNELS_PER_MU: usize = 4;

// ── hse_host.c internal types ─────────────────────────────────────────────────

/// `muInterruptType_t` — selects which MU interrupt line to enable/disable.
///
/// Values from `hse_mu.h`:
///   `HSE_INT_ACK_REQUEST = 0`, `HSE_INT_RESPONSE = 1`, `HSE_INT_SYS_EVENT = 2`.
#[repr(u32)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum MuInterruptType {
    /// TX interrupt — triggered when HSE acknowledges the request.
    AckRequest = 0x00,
    /// RX interrupt — triggered when HSE writes the response.
    Response   = 0x01,
    /// General Purpose interrupt — triggered by HSE system events.
    SysEvent   = 0x02,
}

// ── C global arrays — defined in hse_mu.c ────────────────────────────────────
extern "C" {
    /// `volatile uint32_t muRxEnabledInterruptMask[NUM_MU_INSTANCES]` (hse_mu.c)
    #[allow(non_upper_case_globals)]
    pub static muRxEnabledInterruptMask: [u32; NUM_MU_INSTANCES];

    /// `uint32_t u32BaseAddr[NUM_MU_INSTANCES]` — MU peripheral base addresses (hse_mu.c).
    #[allow(non_upper_case_globals)]
    pub static u32BaseAddr: [u32; NUM_MU_INSTANCES];
}

// ── C functions used by the Rust reimplementations ────────────────────────────
extern "C" {
    // hse_sys_init.h — interrupt masking primitives
    pub fn sys_disableAllInterrupts();
    pub fn sys_enableAllInterrupts();

    // hse_mu.h — MU driver helpers
    pub fn HSE_MU_EnableInterrupts(
        u8_mu_instance: u8,
        mu_interrupt_type: MuInterruptType,
        u32_interrupt_mask: u32,
    );
    pub fn HSE_MU_GetGeneralStatusReg(u8_mu_instance: u8) -> u32;
    pub fn HSE_MU_SetGeneralStatusReg(u8_mu_instance: u8, u32_mask: u32);
    /// Returns non-zero (TRUE) when HSE has written a response on this channel.
    /// `bool_t` = `uint8_t` per `hse_std_typedefs.h`.
    pub fn HSE_MU_IsResponseReady(u8_mu_instance: u8, u8_channel: u8) -> u8;
    /// Reads and returns the RR register for the given channel (non-blocking).
    pub fn HSE_MU_ReceiveResponse(u8_mu_instance: u8, u8_channel: u8) -> u32;
}

// =============================================================================
// LPUART / FlexIO UART driver bindings (Lpuart_Uart_Ip.h / Flexio_Uart_Ip.h)
// =============================================================================

// ── LPUART status codes (Lpuart_Uart_Ip_StatusType) ──────────────────────────

/// Return-status codes of the LPUART IP driver.
///
/// Values taken verbatim from `Lpuart_Uart_Ip_Types.h` (RTD 6.0.0).
#[repr(u32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum LpuartStatus {
    Success      = 0x00,
    Error        = 0x01,
    Busy         = 0x02,
    Timeout      = 0x03,
    TxUnderrun   = 0x04,
    RxOverrun    = 0x05,
    Aborted      = 0x06,
    FramingError = 0x07,
    ParityError  = 0x08,
    NoiseError   = 0x09,
    DmaError     = 0x10,
}

// ── FlexIO UART status codes (Flexio_Uart_Ip_StatusType) ─────────────────────

/// Return-status codes of the FlexIO UART IP driver.
///
/// Values taken verbatim from `Flexio_Uart_Ip_Types.h` (RTD 6.0.0).
#[repr(u32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum FlexioStatus {
    Success    = 0x000,
    Error      = 0x001,
    Busy       = 0x002,
    DmaError   = 0x003,
    TxUnderrun = 0x201,
    RxOverrun  = 0x202,
    Aborted    = 0x204,
    Timeout    = 0x206,
}

// ── Opaque C configuration structs ───────────────────────────────────────────

/// Opaque handle for `Lpuart_Uart_Ip_UserConfigType`.
///
/// Rust never constructs this directly; it always comes from a C-generated
/// `extern` symbol declared by the BSW configuration layer.
#[repr(C)]
pub struct LpuartUserConfig {
    _opaque: [u8; 0],
}

/// Opaque handle for `Flexio_Uart_Ip_UserConfigType`.
///
/// Rust never constructs this directly; it always comes from a C-generated
/// `extern` symbol declared by the BSW configuration layer.
#[repr(C)]
pub struct FlexioUserConfig {
    _opaque: [u8; 0],
}

// ── Lpuart_Uart_Ip.h ─────────────────────────────────────────────────────────
extern "C" {
    /// Initialises an LPUART instance.
    ///
    /// C declaration:
    ///   `void Lpuart_Uart_Ip_Init(uint8 Instance,
    ///                             const Lpuart_Uart_Ip_UserConfigType *UserConfig);`
    pub fn Lpuart_Uart_Ip_Init(instance: u8, user_config: *const LpuartUserConfig);

    /// Disables and deinitialises an LPUART instance.
    ///
    /// C declaration:
    ///   `Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_Deinit(uint8 Instance);`
    pub fn Lpuart_Uart_Ip_Deinit(instance: u8) -> LpuartStatus;

    /// Starts a non-blocking transmit on an LPUART instance.
    ///
    /// C declaration:
    ///   `Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AsyncSend(
    ///       uint8 Instance, const uint8 *TxBuff, uint32 TxSize);`
    pub fn Lpuart_Uart_Ip_AsyncSend(
        instance: u8,
        tx_buff: *const u8,
        tx_size: u32,
    ) -> LpuartStatus;

    /// Returns the status and remaining-byte count of an in-progress LPUART transmit.
    ///
    /// C declaration:
    ///   `Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_GetTransmitStatus(
    ///       uint8 Instance, uint32 *BytesRemaining);`
    pub fn Lpuart_Uart_Ip_GetTransmitStatus(
        instance: u8,
        bytes_remaining: *mut u32,
    ) -> LpuartStatus;

    /// Starts a non-blocking receive on an LPUART instance.
    ///
    /// C declaration:
    ///   `Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AsyncReceive(
    ///       uint8 Instance, uint8 *RxBuff, uint32 RxSize);`
    pub fn Lpuart_Uart_Ip_AsyncReceive(
        instance: u8,
        rx_buff: *mut u8,
        rx_size: u32,
    ) -> LpuartStatus;

    /// Returns the status and remaining-byte count of an in-progress LPUART receive.
    ///
    /// C declaration:
    ///   `Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_GetReceiveStatus(
    ///       uint8 Instance, uint32 *BytesRemaining);`
    pub fn Lpuart_Uart_Ip_GetReceiveStatus(
        instance: u8,
        bytes_remaining: *mut u32,
    ) -> LpuartStatus;
}

// ── Flexio_Uart_Ip.h ─────────────────────────────────────────────────────────
extern "C" {
    /// Initialises a FlexIO UART channel.
    ///
    /// C declaration:
    ///   `void Flexio_Uart_Ip_Init(uint8 Channel,
    ///                             const Flexio_Uart_Ip_UserConfigType *UserConfig);`
    pub fn Flexio_Uart_Ip_Init(channel: u8, user_config: *const FlexioUserConfig);

    /// Disables and deinitialises a FlexIO UART channel.
    ///
    /// C declaration:
    ///   `Flexio_Uart_Ip_StatusType Flexio_Uart_Ip_Deinit(uint8 Channel);`
    pub fn Flexio_Uart_Ip_Deinit(channel: u8) -> FlexioStatus;

    /// Starts a non-blocking transmit on a FlexIO UART channel.
    ///
    /// C declaration:
    ///   `Flexio_Uart_Ip_StatusType Flexio_Uart_Ip_AsyncSend(
    ///       uint8 Channel, const uint8 *TxBuff, uint32 TxSize);`
    pub fn Flexio_Uart_Ip_AsyncSend(
        channel: u8,
        tx_buff: *const u8,
        tx_size: u32,
    ) -> FlexioStatus;

    /// Returns the status and remaining-byte count of an in-progress FlexIO UART transfer.
    /// Used for both TX and RX channels.
    ///
    /// C declaration:
    ///   `Flexio_Uart_Ip_StatusType Flexio_Uart_Ip_GetStatus(
    ///       uint8 Channel, uint32 *BytesRemaining);`
    pub fn Flexio_Uart_Ip_GetStatus(channel: u8, bytes_remaining: *mut u32) -> FlexioStatus;

    /// Starts a non-blocking receive on a FlexIO UART channel.
    ///
    /// C declaration:
    ///   `Flexio_Uart_Ip_StatusType Flexio_Uart_Ip_AsyncReceive(
    ///       uint8 Channel, uint8 *RxBuff, uint32 RxSize);`
    pub fn Flexio_Uart_Ip_AsyncReceive(
        channel: u8,
        rx_buff: *mut u8,
        rx_size: u32,
    ) -> FlexioStatus;
}

// =============================================================================
// HSE secure-communication bindings
// =============================================================================

// ── Key-type constant (hse_keymgmt_common_types.h) ────────────────────────────

/// `HSE_KEY_TYPE_AES` — AES symmetric key type (`hseKeyType_t`).
pub const HSE_KEY_TYPE_AES: u8 = 0x12;

// ── Key-usage flags (`hseKeyFlags_t` — u16) ──────────────────────────────────

/// `HSE_KF_USAGE_ENCRYPT` — key may be used for data encryption.
pub const HSE_KF_USAGE_ENCRYPT: u16 = 1 << 0;
/// `HSE_KF_USAGE_DECRYPT` — key may be used for data decryption.
pub const HSE_KF_USAGE_DECRYPT: u16 = 1 << 1;
/// `HSE_KF_USAGE_SIGN` — key may be used to generate MACs.
pub const HSE_KF_USAGE_SIGN: u16 = 1 << 2;
/// `HSE_KF_USAGE_VERIFY` — key may be used to verify MACs.
pub const HSE_KF_USAGE_VERIFY: u16 = 1 << 3;

// ── AES block-mode mask (`hseAesBlockModeMask_t` — u8) ───────────────────────

/// `HSE_KU_AES_BLOCK_MODE_CBC` — restricts key use to CBC mode.
///
/// Value: `1 << HSE_CIPHER_BLOCK_MODE_CBC` = `1 << 2` = `4`.
pub const HSE_KU_AES_BLOCK_MODE_CBC: u8 = 1 << 2;

// ── Cipher block-mode selector (`hseCipherBlockMode_t` — u8) ─────────────────

/// `HSE_CIPHER_BLOCK_MODE_CBC` — selects CBC mode in `AesEncrypt` / `AesDecrypt`.
pub const HSE_CIPHER_BLOCK_MODE_CBC: u8 = 2;

// ── Scatter-gather option ─────────────────────────────────────────────────────

/// `HSE_SGT_OPTION_NONE` — no scatter-gather table; plain contiguous buffer.
pub const HSE_SGT_OPTION_NONE: u8 = 0;

// ── RAM AES key-handle constants ──────────────────────────────────────────────
//
// `GET_KEY_HANDLE(catalogId, groupIdx, slotIdx)`
//   = (catalogId << 16) | (groupIdx << 8) | slotIdx
// HSE_KEY_CATALOG_ID_RAM = 2, group-1 = AES-128 RAM group (10 slots).

/// `GET_KEY_HANDLE(RAM=2, group=1, slot=0)` — secure-comm CBC cipher key.
pub const RAM_AES_GROUP1_SLOT0: u32 = (2u32 << 16) | (1u32 << 8) | 0;

/// `GET_KEY_HANDLE(RAM=2, group=1, slot=1)` — secure-comm CMAC key.
pub const RAM_AES_GROUP1_SLOT1: u32 = (2u32 << 16) | (1u32 << 8) | 1;

/// `GET_KEY_HANDLE(RAM=2, group=1, slot=2)` — req_save_data / req_saved_data HMAC-SHA-256 key.
pub const RAM_HMAC_GROUP1_SLOT2: u32 = (2u32 << 16) | (1u32 << 8) | 2;

// ── hse_host_import_key.h ─────────────────────────────────────────────────────

extern "C" {
    /// Imports a raw (plain) symmetric key into an HSE RAM key slot.
    ///
    /// - `target_key_handle`: destination slot (`RAM_AES_GROUP1_SLOT0` etc.)
    /// - `key_type`:          `HSE_KEY_TYPE_AES` (0x12)
    /// - `key_flags`:         bitmask of `HSE_KF_USAGE_*` flags
    /// - `key_byte_length`:   key size in bytes (16 for AES-128)
    /// - `p_key`:             pointer to raw key bytes
    /// - `cipher_mode_mask`:  allowed block-mode mask; 0 = any mode allowed
    ///
    /// C declaration:
    ///   `hseSrvResponse_t ImportPlainSymKeyReq(
    ///       hseKeyHandle_t targetKeyHandle, hseKeyType_t keyType,
    ///       hseKeyFlags_t keyFlags, uint16_t keyByteLength,
    ///       const uint8_t *pKey, hseAesBlockModeMask_t cipherModeMask);`
    pub fn ImportPlainSymKeyReq(
        target_key_handle: u32,
        key_type: u8,
        key_flags: u16,
        key_byte_length: u16,
        p_key: *const u8,
        cipher_mode_mask: u8,
    ) -> HseSrvResponse;
}

// ── hse_host_cipher.h ─────────────────────────────────────────────────────────

extern "C" {
    /// AES one-pass encrypt (`HSE_SRV_ID_SYM_CIPHER`, direction = ENCRYPT).
    ///
    /// - `cipher_block_mode`: `HSE_CIPHER_BLOCK_MODE_CBC` (2) for CBC-128
    /// - `p_iv`:              pointer to a 16-byte IV (consumed, not modified)
    /// - `input_length`:      must be a non-zero multiple of 16
    /// - `input_sgt_type`:    `HSE_SGT_OPTION_NONE` (0) for plain buffers
    ///
    /// C declaration:
    ///   `hseSrvResponse_t AesEncrypt(hseKeyHandle_t keyHandle,
    ///       hseCipherBlockMode_t cipherBlockMode, const uint8_t *pIV,
    ///       uint32_t inputLength, const uint8_t *pInput, uint8_t *pOutput,
    ///       hseSGTOption_t inputSgtType);`
    pub fn AesEncrypt(
        key_handle: u32,
        cipher_block_mode: u8,
        p_iv: *const u8,
        input_length: u32,
        p_input: *const u8,
        p_output: *mut u8,
        input_sgt_type: u8,
    ) -> HseSrvResponse;

    /// AES one-pass decrypt (`HSE_SRV_ID_SYM_CIPHER`, direction = DECRYPT).
    ///
    /// Same parameters as `AesEncrypt`; `p_iv` must be the same IV used during
    /// encryption.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t AesDecrypt(hseKeyHandle_t keyHandle,
    ///       hseCipherBlockMode_t cipherBlockMode, const uint8_t *pIV,
    ///       uint32_t inputLength, const uint8_t *pInput, uint8_t *pOutput,
    ///       hseSGTOption_t inputSgtType);`
    pub fn AesDecrypt(
        key_handle: u32,
        cipher_block_mode: u8,
        p_iv: *const u8,
        input_length: u32,
        p_input: *const u8,
        p_output: *mut u8,
        input_sgt_type: u8,
    ) -> HseSrvResponse;
}

// ── hse_host_mac.h ────────────────────────────────────────────────────────────

extern "C" {
    /// Generates an AES-CMAC-128 tag (`HSE_SRV_ID_MAC`, direction = GENERATE).
    ///
    /// - `msg_length`:   message length **in bytes**
    /// - `p_tag_length`: on entry, size of the tag buffer (16 for full CMAC-128);
    ///                   updated to actual tag length on exit
    ///
    /// C declaration:
    ///   `hseSrvResponse_t AesCmacGenerate(hseKeyHandle_t keyHandle,
    ///       uint32_t msgLength, const uint8_t *pMsg, uint32_t *pTagLength,
    ///       uint8_t *pTag, hseSGTOption_t inputSgtType);`
    pub fn AesCmacGenerate(
        key_handle: u32,
        msg_length: u32,
        p_msg: *const u8,
        p_tag_length: *mut u32,
        p_tag: *mut u8,
        input_sgt_type: u8,
    ) -> HseSrvResponse;

    /// Verifies an AES-CMAC-128 tag (`HSE_SRV_ID_MAC`, direction = VERIFY).
    ///
    /// Returns `HSE_SRV_RSP_OK` on tag match, a non-OK code on any mismatch
    /// or error (including `HSE_SRV_RSP_VERIFY_FAILED` = `0x00A5_5AA5`).
    ///
    /// C declaration:
    ///   `hseSrvResponse_t AesCmacVerify(hseKeyHandle_t keyHandle,
    ///       uint32_t msgLength, const uint8_t *pMsg, const uint32_t *tagLength,
    ///       const uint8_t *pTag, hseSGTOption_t inputSgtType);`
    pub fn AesCmacVerify(
        key_handle: u32,
        msg_length: u32,
        p_msg: *const u8,
        tag_length: *const u32,
        p_tag: *const u8,
        input_sgt_type: u8,
    ) -> HseSrvResponse;
}

// ── hse_b_catalog_formatting.h ────────────────────────────────────────────────

extern "C" {
    /// Formats the NVM and RAM key catalogs using the built-in demo catalog
    /// configuration (`HSE_DEMO_NVM_KEY_CATALOG_CFG` / `HSE_DEMO_RAM_KEY_CATALOG_CFG`),
    /// then initialises the software key-handle allocator (HKF).
    ///
    /// Internally calls `FormatKeyCatalogs(nvmKeyCatalog, ramKeyCatalog)`
    /// followed by `ParseKeyCatalogs()` (= `HKF_Init`).
    /// Only meaningful when `HSE_SPT_FORMAT_KEY_CATALOGS` is defined.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t FormatKeyCatalogs_(void);`
    pub fn FormatKeyCatalogs_() -> HseSrvResponse;

    /// Initialises the software key-handle allocator from an already-formatted
    /// catalog configuration.
    ///
    /// Equivalent to `HKF_Init(nvmKeyCatalog, ramKeyCatalog)`.
    /// Safe to call on every boot even when the catalog was formatted in a
    /// previous boot cycle.
    ///
    /// C declaration:
    ///   `hseSrvResponse_t ParseKeyCatalogs(void);`
    pub fn ParseKeyCatalogs() -> HseSrvResponse;

    /// Returns non-zero (`TRUE`) when the HSE firmware reports that the key
    /// catalogs have been successfully installed (`HSE_STATUS_INSTALL_OK` bit).
    ///
    /// `bool_t` = `uint8_t` per `hse_platform.h`.
    ///
    /// C declaration:
    ///   `bool_t IsKeyCatalogFormatted(void);`
    pub fn IsKeyCatalogFormatted() -> u8;
}
