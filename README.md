# HSE Library - Extracted from S32K344 Template Project

A ready-to-use hardware security engine (HSE) library extracted directly from the NXP S32K344 Template project. This library contains all the actual working HSE host implementation code and cryptographic service examples.

---

## Quick Start — NXP S32DS Integration

### 1. Build the library (run once, or after any source change)

Open a terminal with the S32DS ARM GCC toolchain in PATH (or use the S32DS
"Build" button on a CMake project), then:

```sh
cd HSELib/build
cmake .. \
  -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
  "-DCMAKE_C_FLAGS=-mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -Os -g3 \
   -ffunction-sections -fdata-sections --specs=nano.specs"
make -j$(nproc)
# Output: HSELib/build/libhselib.a
```

### 2. Add to your S32DS project

In **Project → Properties → C/C++ Build → Settings → ARM GCC C Compiler**:

| Setting | Value |
|---|---|
| **Includes → Include paths (`-I`)** | `<path-to-HSELib>/include` |

> Add this **after** the S32DS / MCU SDK include paths so S32DS platform headers
> take precedence for any shared files (Std_Types.h, core_cm7.h, etc.).

In **Linker → Libraries**:

| Setting | Value |
|---|---|
| **Libraries (`-l`)** | `hselib` |
| **Library search path (`-L`)** | `<path-to-HSELib>/build` |

### 3. In your application code

```c
#include "hselib.h"   // one include gives you everything
```

#### Conflict resolution

All AUTOSAR / CMSIS platform headers in `HSELib/include/` (`Std_Types.h`,
`Platform_Types.h`, `Compiler.h`, `core_cm7.h`, `S32K344_*.h`, …) are
**identical** to the ones shipped with S32DS — their include guards
(`STD_TYPES_H`, `PLATFORM_TYPES_H`, `__CORE_CM7_H_GENERIC`, …) make the
second inclusion a no-op, so there is no conflict in practice.

If you ever see a redefinition error despite the guards, add a global
compiler define in S32DS:

```
-DHSE_SKIP_PLATFORM_HEADERS   # suppress all AUTOSAR/CMSIS/device headers from HSELib
-DHSE_SKIP_DEVICE_HEADERS     # suppress only S32K344_*.h headers from HSELib
```

### 4. What is NOT in the library

| File | Reason excluded |
|---|---|
| `sys_init.c` | Defines `SVC_Handler` / `PendSV_Handler` / `SysTick_Handler` — conflicts with FreeRTOS |
| `nvic.c` | Requires AUTOSAR `Os.h`, not available in standalone builds |
| `hse_crypto.c` | Demo-only application code, not part of the re-usable API |

---

## HSE MU Interrupt Integration (AUTOSAR RTD projects)

`libhselib.a` ships two files that handle interrupt wiring automatically for
projects whose startup code matches the RTD 6.x pattern
(`startup_cm7.s` / `Vector_Table.s` / `system.c`):

| File | What it provides |
|---|---|
| `src/hse_irq_integration.c` | ISR wrappers (`HSE_MU0_RxIRQHandler` etc.) + `HSE_RegisterMuInterrupts()` |
| `include/hse_irq_integration.h` | Declarations and integration notes |
| `src/hse_sys_init.c` | Weak `sys_disableAllInterrupts` / `sys_enableAllInterrupts` (PRIMASK-based) |

**Only two manual steps are required per project:**

### Step A — Wire the ISRs into Vector_Table.s

Open your project's `Vector_Table.s`.

**1.** Add four `.globl` lines near the top, after the existing `SysTick_Handler` entry:

```asm
.globl SysTick_Handler           /* SysTick Handler */ /* 15*/
.globl HSE_MU0_RxIRQHandler
.globl HSE_MU0_OredIRQHandler
.globl HSE_MU1_RxIRQHandler
.globl HSE_MU1_OredIRQHandler
```

**2.** Replace the four `undefined_handler` entries at IRQ 193–197 in the
peripheral vector table (the block immediately before the `/*200*/` comment):

```asm
/* before — leave IRQs 190-192 unchanged */
.long undefined_handler          /* IRQ 190 */
.long undefined_handler          /* IRQ 191 */
.long undefined_handler          /* IRQ 192 */
/* change these four: */
.long HSE_MU0_RxIRQHandler       /* IRQ 193  HSE_MU0_RX_IRQn   */
.long HSE_MU0_OredIRQHandler     /* IRQ 194  HSE_MU0_ORED_IRQn */
.long undefined_handler          /* IRQ 195  (HSE_MU1_TX, unused) */
.long HSE_MU1_RxIRQHandler       /* IRQ 196  HSE_MU1_RX_IRQn   */
.long HSE_MU1_OredIRQHandler     /* IRQ 197  HSE_MU1_ORED_IRQn */
/* leave IRQs 198+ unchanged */
.long undefined_handler          /* IRQ 198 */
.long undefined_handler          /* IRQ 199 */
.long undefined_handler /*200*/
```

> **Finding the right block:** The vector table in `Vector_Table.s` lists
> peripheral IRQs as `.long undefined_handler` entries in order starting at
> IRQ 0. IRQ 193 is the 194th `.long` entry in the peripheral section (after
> the 16 exception entries). Count from `/*0*/` or search for the `/*190*/`
> comment if present.

### Step B — Enable the interrupts at runtime

Call `HSE_RegisterMuInterrupts()` **once**, before issuing any HSE service
in interrupt-driven (asynchronous or sync-with-irq) mode. The earliest
safe point is after `startup_cm7.s` has set `VTOR` to the RAM vector table
(`SET_VTOR_TCM`) and called `SystemInit()`:

```c
#include "hselib.h"   /* hse_irq_integration.h is already included by hselib.h */

int main(void)
{
    HSE_RegisterMuInterrupts();   /* enable MU0 + MU1 RX/ORED IRQs in NVIC */

    /* ... rest of HSE initialisation, e.g. HSE_SendFormatKeyCatalogs() ... */
}
```

The function directly writes the Cortex-M7 `NVIC_ISER` and `NVIC_IP`
registers — no dependency on any project-specific `nvic.c` file.

#### Interrupt priority

The default priority is **1** (second highest on S32K344's 0–15 scale).
Override it with a compiler define **before** `hselib.h` is first included:

```c
#define HSE_MU_IRQ_PRIORITY  4   /* or any value 0-15 */
#include "hselib.h"
```

Or add `-DHSE_MU_IRQ_PRIORITY=4` to your compiler flags.

> **AUTOSAR OS note:** If your project uses AUTOSAR OS with `BASEPRI`-based
> interrupt locking, ensure `HSE_MU_IRQ_PRIORITY` is **numerically lower**
> (higher urgency) than the `basepri` threshold, otherwise the MU interrupts
> will be masked while the OS holds a resource lock and HSE sync-with-irq
> mode will deadlock.

### Step C — D-cache (AUTOSAR RTD projects only)

The NXP demo projects run with D-cache **disabled** and therefore have no
cache-coherency issues.  AUTOSAR RTD projects typically enable D-cache in
`system.c` (`SCB_EnableDCache()`).  Because the HSE shared SRAM window at
`0x22C00000` is accessed by both the Cortex-M7 (cached) and the HSE DMA
master (uncached), service descriptors and data buffers written into that
window may stay in the D-cache and never reach actual SRAM before HSE reads
them.  The symptom is intermittent `HSE_SRV_RSP_NOT_SUPPORTED` (or similar)
failures from services such as `FormatKeyCatalogs` that disappear when
stepping through the code in a debugger.

**Recommended fix — disable D-cache before using HSE:**

```c
int main(void)
{
    SCB_DisableDCache();          /* must come before any HSE service call */
    HSE_RegisterMuInterrupts();
    WaitForHSEFWInitToFinish();   /* or HSE_WaitForInitOk() */
    /* ... */
}
```

If your application requires D-cache for performance, add a dedicated
Non-Cacheable MPU region (`TEX=1 C=0 B=0`) covering `0x22C00000` with a
higher priority (lower index number) than any surrounding SRAM region.  This
allows the rest of SRAM to remain cached while the HSE shared window stays
coherent.

---

### Polling mode (no interrupt wiring needed)

If you use only polled HSE calls (`gSyncTxOption` with interrupts disabled),
you can skip Steps A and B entirely.  `HSE_Send()` will fall back to
busy-waiting on the MU Receive Status Register whenever the MU RX interrupt
is not enabled for the channel being used.  Call `HSE_RegisterMuInterrupts()`
only when asynchronous or sync-with-irq operation is required.

---

## What This Library Contains

A standalone **HSE (Hardware Security Engine) Library** extracted directly from the
NXP S32K344 Template project. This is **not a custom wrapper** – it contains the
**actual working production code** from the Template. The directory layout mirrors
the original Template project so you can drop it into any demo without changing
build rules.

- **Core HSE implementation (`lib/`):**
  - `host_hse/` – MU communication and service implementations
  - `host_keymgmt/` – key management helpers
  - `host_crypto_helper/` – crypto utilities
  - `services/` – example service code (`fw_crypto`, `standard`, …) and
    **application‑level helper headers** (global_defs.h, hse_config.h,
    hse_monotonic_cnt.h, etc.) that the examples use.
  - `include/` – internal headers
  - `src/` – common source files

- **Hardware drivers (`drivers/`):**
  - `mu/` – Message Unit driver used for HSE requests
  - `stm/` – System Timer Module driver
  - `nvic/`, `dcm_register/` – support helpers

- **Platform interface (`interface/`)** – firmware definitions, types,
  status flags, service descriptors, etc.

- **Device headers (`include/`)** – full set of S32K344 CMSIS and peripheral
  headers provided alongside the library sources.

- **Examples & docs** – see `lib/services/fw_crypto/hse_crypto.c` for 2200+
  lines of real examples; `test/src/hse_library_example.c` contains a
  simplified application.

## File Structure

```
HSELib/
├── lib/
│   ├── host_hse/
│   ├── host_keymgmt/
│   ├── host_crypto_helper/
│   ├── services/
│   ├── include/
│   └── src/
├── drivers/
│   ├── mu/
│   ├── stm/
│   ├── nvic/
│   └── dcm_register/
├── interface/
├── include/                    # S32K344 device headers + CMSIS
├── test/
│   └── src/
│       └── hse_library_example.c
└── [documentation files]
```

## Next Steps

1. Copy HSELib to your project
2. Add include paths to your build configuration (see integration guide)
3. Add source files to compile list
4. Review examples in `lib/services/fw_crypto/hse_crypto.c`
5. Implement your application using the HSE service interface

## Building the Library with CMake

A ready‑made `CMakeLists.txt` lives at the root of this directory.  Running
`cmake` will produce a static archive named `libhselib.a` in the
`build/` subdirectory; the following commands illustrate a simple build:

```sh
mkdir -p build && cd build
cmake ..
make
```

Once the archive exists you can link to `hselib` from your own CMake project,
as described in the integration guide.  The CMake script automatically adds
all required include paths and drivers, and it deliberately **does not compile
service demo sources** (such as `fw_crypto/hse_crypto.c`) since they include
application-specific initializations that aren’t needed in a generic library.
You may compile those examples separately if you wish.

## Support Files

- Refer to `lib/services/fw_crypto/hse_crypto.c` for complete examples
- Check `lib/host_hse/hse_host.h` for API structures
- Review `lib/services/inc/hse_demo_app_services.h` for service definitions

### Quick Start - Link with Your Project

1. **Copy library to your project:**
   ```bash
   cp -r HSELib /path/to/your/project/
   ```

2. **Include HSE headers in your code:**
   ```c
   #include "hse_host.h"
   #include "hse_interface.h"
   #include "hse_host_cipher.h"
   #include "hse_host_mac.h"
   // ... other headers as needed
   ```
   *(all library headers now reside under `include/hse/` but the parent `include/` path is sufficient since it contains both device and HSE headers)*

3. **Build with library files:**
   
   **S32DS IDE**: Add the top‑level `HSELib/include/` and `HSELib/src/` folders to your project

   **CMake**:
   ```cmake
   # Include paths (single include folder covers everything)
   include_directories(
       ${CMAKE_CURRENT_SOURCE_DIR}/HSELib/include
   )
   
   # Add source files from the unified src directory
   file(GLOB_RECURSE HSE_SOURCES
       "${CMAKE_CURRENT_SOURCE_DIR}/HSELib/src/**/*.c"
   )
   
   target_sources(${PROJECT_NAME} PRIVATE ${HSE_SOURCES})
   ```

   **Makefile**:
   ```makefile
   # Single include path
   INCLUDES += -I$(HSELIB_DIR)/include
   
   # Compile all source files beneath src/
   SOURCES += $(shell find $(HSELIB_DIR)/src -name "*.c")
   ```


### Example Code

#### 1. AES Encryption (from hse_crypto.c pattern)

```c
#include "hse_host.h"
#include "hse_host_cipher.h"

// After HSE is initialized...
uint8_t plaintext[16] = { /* ... */ };
uint8_t ciphertext[16];
uint8_t key[32] = { /* ... */ };

// Create AES cipher request
hseSrvDescriptor_t* pHseSrvDesc = gHseSrvDesc[MU0];

pHseSrvDesc->srvId = HSE_SRV_CIPHER;
pHseSrvDesc->srvPayload.cipherReq.cipherKeyHandle = key_handle;
pHseSrvDesc->srvPayload.cipherReq.cipherAlgo = HSE_CIPHER_ALGO_AES;
pHseSrvDesc->srvPayload.cipherReq.cipherMode = HSE_CIPHER_MODE_ECB;
pHseSrvDesc->srvPayload.cipherReq.cipherDir = HSE_CIPHER_DIR_ENCRYPT;
pHseSrvDesc->srvPayload.cipherReq.inputLength = sizeof(plaintext);
pHseSrvDesc->srvPayload.cipherReq.pIn = PTR_TO_HOST_ADDR(plaintext);
pHseSrvDesc->srvPayload.cipherReq.pOut = PTR_TO_HOST_ADDR(ciphertext);

// Send request
uint8_t u8MuChannel = HSE_GetFreeChannel(MU0);
hseSrvResponse_t response = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);

if (HSE_SRV_RSP_OK == response) {
    printf("Encryption successful\n");
}
```

#### 2. SHA-256 Hashing

```c
#include "hse_host.h"
#include "hse_host_sipher.h"  // Includes hash functions

uint8_t message[] = "Hello, World!";
uint8_t hash[32];

pHseSrvDesc->srvId = HSE_SRV_HASH;
pHseSrvDesc->srvPayload.hashReq.hashAlgo = HSE_HASH_ALGO_SHA2_256;
pHseSrvDesc->srvPayload.hashReq.inputLength = sizeof(message);
pHseSrvDesc->srvPayload.hashReq.pIn = PTR_TO_HOST_ADDR(message);
pHseSrvDesc->srvPayload.hashReq.pHashLength = PTR_TO_HOST_ADDR(&hash); // Will contain hash

uint8_t channel = HSE_GetFreeChannel(MU0);
hseSrvResponse_t response = HSE_Send(MU0, channel, gSyncTxOption, pHseSrvDesc);
```

#### 3. CMAC Generation

```c
#include "hse_host.h"
#include "hse_host_mac.h"

uint8_t message[] = "Message to authenticate";
uint8_t tag[16];
uint8_t key[16] = { /* ... */ };

pHseSrvDesc->srvId = HSE_SRV_MAC;
pHseSrvDesc->srvPayload.macReq.macAlgo = HSE_MAC_ALGO_CMAC;
pHseSrvDesc->srvPayload.macReq.macKeyHandle = key_handle;
pHseSrvDesc->srvPayload.macReq.inputLength = sizeof(message);
pHseSrvDesc->srvPayload.macReq.pIn = PTR_TO_HOST_ADDR(message);
pHseSrvDesc->srvPayload.macReq.pTag = PTR_TO_HOST_ADDR(tag);
pHseSrvDesc->srvPayload.macReq.tagLength = sizeof(tag);

uint8_t channel = HSE_GetFreeChannel(MU0);
hseSrvResponse_t response = HSE_Send(MU0, channel, gSyncTxOption, pHseSrvDesc);
```

## Key Files

| File/Directory | Purpose |
|---|---|
| `lib/host_hse/hse_host.c` | Core HSE host communication |
| `lib/host_hse/hse_host_*.c` | Service implementations |
| `lib/services/fw_crypto/hse_crypto.c` | Crypto service examples |
| `lib/host_keymgmt/` | Key management functions |
| `drivers/mu/` | Message Unit driver |
| `drivers/stm/` | System Timer driver |
| `interface/` | HSE interface definitions |

## Integration with Other Projects

This library can be used with any project in the demo suite:

- **S32K344_AES_EncryptDecrypt** - Direct code replacement
- **S32K344_CMAC_GenVer** - Use MAC service functions
- **S32K344_Hse_Ecc_Example** - Use sign/verify functions
- **S32K344_Basic_SecureBoot** - Use hash and sign functions
- **S32K344_Advanced_SecureBoot** - Full crypto support

Simply add the HSELib include paths and source files to your project's build configuration.

## Supported Operations

### Symmetric Cryptography
- AES (ECB, CBC, CTR modes)
- DES (not recommended)
- Serpent

### Asymmetric Cryptography
- RSA (1024, 2048, 3072, 4096-bit)
- ECDSA (SECP256R1, SECP384R1, etc.)
- ECDH

### Hash Functions
- SHA-1
- SHA-256, SHA-384, SHA-512
- SM3

### Message Authentication
- CMAC
- HMAC
- GMAC

### Key Derivation
- KDF (SP 800-108, SP 800-56C)

### Additional Services
- Random number generation
- Monotonic counters
- Secure boot services

## Building and Testing

To test the library with the included service examples:

```bash
# Copy hse_crypto.c from lib/services/fw_crypto/ to your project
# Include all HSE lib paths
# Build with your toolchain
# The examples in hse_crypto.c demonstrate each service
```

## Architecture Notes

- **No Modifications**: All code is copied directly from NXP Template
- **Real Implementation**: These are production-ready cryptographic services
- **Hardware Dependent**: Requires S32K344 hardware (or simulator)
- **Service-Based**: Uses HSE service descriptor interface
- **MU Communication**: Uses Message Unit for HSE firmware communication

## File Structure

```
HSELib/
├── lib/
│   ├── host_hse/              # HSE host communication layer
│   ├── host_keymgmt/          # Key management
│   ├── host_crypto_helper/    # Crypto helper functions
│   ├── services/              # Service examples (fw_crypto, etc.)
│   ├── include/               # Internal headers
│   └── src/                   # Common source files
├── drivers/                   # Hardware-specific drivers
│   ├── mu/                    # Message Unit driver
│   ├── stm/                   # System Timer driver
│   └── ...
├── interface/                 # HSE firmware interface
├── include/                   # Platform headers (S32K344.h, etc.)
└── [This README]
```

## Next Steps

1. Copy HSELib to your project
2. Add include paths to your build configuration
3. Add source files to compile list
4. Review examples in `lib/services/fw_crypto/hse_crypto.c`
5. Implement your application using the HSE service interface

## Support Files

- Refer to `lib/services/fw_crypto/hse_crypto.c` for complete examples
- Check `lib/host_hse/hse_host.h` for API structures
- Review `lib/services/inc/hse_demo_app_services.h` for service definitions

---

## Rust Communication Middleware

The `rust/` subdirectory contains a Rust static library (`libhselib_rust.a`) that provides two layers of UART communication: a plain transport layer and a secure channel with hardware-backed AES-CBC-128 encryption and AES-CMAC-128 authentication.

The library is split into two Rust modules:

* **`hse.rs`** — pure-Rust MMIO driver that communicates directly with the HSE firmware over MU0 hardware registers.  No FFI to any C host-wrapper.  Provides all six crypto service functions.
* **`communication_middleware.rs`** — UART transport with an optional secure channel.  Delegates crypto to `hse.rs`; UART I/O goes through the C RTD driver via FFI.

### Overall Architecture

Full call-chain from C application code down to HSE firmware:

```
┌──────────────────────────────────────────────────────────────┐
│                     C Application Code                        │
│           hse_she_command_main.c / user application           │
└─────────────────────────┬────────────────────────────────────┘
                          │  C-callable exports  (hse_rust.h)
┌─────────────────────────┴────────────────────────────────────┐
│               Rust: communication_middleware                   │
│                                                               │
│   ┌───────────────────────────────────────────────────────┐   │
│   │                  Secure Channel Layer                  │   │
│   │   secure_comm_init_she_keys()                         │   │
│   │   secure_send_to_ap_encrypted()                       │   │
│   │   secure_recv_from_ap_encrypted()                     │   │
│   └──────────┬──────────────────────────────────────────┬─┘   │
│              │ hse::* (safe Rust API)  │ plain transport │     │
│   ┌──────────┴────────────────┐   ┌───┴────────────────┐ │    │
│   │   Plain Transport Layer   │   │   [crypto via hse] │ │    │
│   │  send_to_ap / recv_from_ap│   │                    │ │    │
│   └──────────────┬────────────┘   └────────────────────┘ │    │
└──────────────────┼────────────────────────────────────────────┘
                   │                               │
      unsafe FFI (ffi.rs)                 hse::* safe calls
      UART drivers only                         │
┌─────────────────┴──────┐   ┌─────────────────┴────────────────┐
│     C RTD UART Layer   │   │       Rust: hse (MMIO driver)     │
│                        │   │                                   │
│  Lpuart_Uart_Ip_*      │   │  import_plain_sym_key()           │
│  Flexio_Uart_Ip_*      │   │  aes_cbc_encrypt()                │
│                        │   │  aes_cbc_decrypt()                │
│  (catalog FFI only:)   │   │  aes_cmac_generate()              │
│  FormatKeyCatalogs_()  │   │  aes_cmac_verify()                │
│  IsKeyCatalogFormatted │   │  get_rng_drg4()                   │
│  ParseKeyCatalogs()    │   │                                   │
└────────────────────────┘   └─────────────────┬────────────────┘
                                               │ unsafe volatile MMIO
                             ┌─────────────────┴────────────────┐
                             │       MU0 Hardware Registers      │
                             │   base 0x4038_C000                │
                             │   TR[1] / RR[1] / FSR / TSR / RSR │
                             └─────────────────┬────────────────┘
                                               │ HSE MU protocol
                             ┌─────────────────┴────────────────┐
                             │        HSE_B Firmware             │
                             │  SYM_CIPHER  MAC  GET_RANDOM      │
                             │  IMPORT_KEY                       │
                             └───────────────────────────────────┘
```

---

## HSE Rust MMIO Driver (`hse.rs`)

`rust/src/hse.rs` is a self-contained, pure-Rust implementation of six HSE
cryptographic service calls.  It communicates with the HSE firmware entirely
through direct volatile reads and writes to the MU0 peripheral registers — no
FFI calls to any C host-wrapper function are made.

### Why a pure-Rust driver?

| Concern | `ffi.rs` approach | `hse.rs` approach |
|---|---|---|
| Dependency on C build | requires `libhselib.a` symbols | none — MMIO only |
| Safety surface | every C call is `unsafe` | only MMIO reads/writes are `unsafe` |
| Error type | `HseSrvResponse` (u32 newtype) | `HseError` enum |
| Exported symbols | C-mangled names | name-mangled Rust symbols |

### Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      Rust: hse (hse.rs)                           │
│                                                                   │
│  Public safe API (name-mangled, not extern "C"):                  │
│                                                                   │
│  ┌───────────────────────┐  ┌──────────────────────────────────┐ │
│  │  Symmetric Cipher     │  │  CMAC  (AES-based)               │ │
│  │  aes_cbc_encrypt()    │  │  aes_cmac_generate()             │ │
│  │  aes_cbc_decrypt()    │  │  aes_cmac_verify()               │ │
│  └───────────┬───────────┘  └─────────────┬────────────────────┘ │
│                                            │                      │
│  ┌───────────────────────┐  ┌──────────────┴─────────────────┐   │
│  │  HMAC  (hash-based)   │  │  Key Management                │   │
│  │  hmac_generate()      │  │  import_plain_sym_key()  (AES) │   │
│  │    └─ HseHashAlgo:    │  │  import_plain_hmac_key() (HMAC)│   │
│  │       Sha1 / 224 /    │  └─────────────┬──────────────────┘   │
│  │       256 / 384 / 512 │                │                      │
│  │  hmac_verify()        │  ┌─────────────┴──────────────────┐   │
│  └───────────┬───────────┘  │  RNG                           │   │
│              │              │  get_rng_drg4()                │   │
│              │              └─────────────┬──────────────────┘   │
│              └────────────────────────────┘                      │
│                             │                                     │
│               hse_send()  ──┘   (Desc: 256-byte aligned buf)     │
│               channel_busy()    (FSR / TSR / RSR poll)           │
└─────────────────────────────┬────────────────────────────────────┘
                              │ unsafe volatile read/write
              ┌───────────────┴─────────────────────────────────┐
              │              MU0 Registers  (0x4038_C000)        │
              │  FSR +0x104  TSR +0x124  RSR +0x12C             │
              │  TR[1] +0x204 (send descriptor address)          │
              │  RR[1] +0x284 (receive hseSrvResponse_t)         │
              └───────────────┬─────────────────────────────────┘
                              │ HSE MU protocol (channel 1)
              ┌───────────────┴─────────────────────────────────┐
              │              HSE_B Firmware                       │
              │  0x00A5_0203  SYM_CIPHER  (AES-CBC enc/dec)      │
              │  0x00A5_0201  MAC         (CMAC gen/verify,       │
              │                           HMAC gen/verify)        │
              │  0x0000_0300  GET_RANDOM  (DRG4)                  │
              │  0x0000_0104  IMPORT_KEY  (AES or HMAC key)       │
              └─────────────────────────────────────────────────┘
```

### MU0 Register Map Used

| Rust constant | Address | Role |
|---|---|---|
| `MU_FSR` | `0x4038_C104` | Flag Status Register — bit[ch] set = HSE busy |
| `MU_TSR` | `0x4038_C124` | TX Status — bit[ch] set = channel ready to send |
| `MU_RSR` | `0x4038_C12C` | RX Status — bit[ch] set = response waiting |
| `MU_TR1` | `0x4038_C204` | Transmit Register ch. 1 — write descriptor address |
| `MU_RR1` | `0x4038_C284` | Receive Register ch. 1 — read `hseSrvResponse_t` |

All calls use **MU0, channel 1** — the same channel used by all C host wrappers.

### Service IDs and Response Codes

| Constant | Value | Purpose |
|---|---|---|
| `SRV_ID_SYM_CIPHER` | `0x00A5_0203` | AES-CBC encrypt / decrypt |
| `SRV_ID_MAC` | `0x00A5_0201` | AES-CMAC **and** HMAC generate / verify |
| `SRV_ID_GET_RANDOM` | `0x0000_0300` | DRG4 random number |
| `SRV_ID_IMPORT_KEY` | `0x0000_0104` | Plain symmetric key import (AES or HMAC) |
| `HSE_SRV_RSP_OK` | `0x55A5_AA33` | Success |
| `HSE_SRV_RSP_VERIFY_FAILED` | `0x55A5_A164` | CMAC / HMAC tag mismatch |

### Synchronous Send Flow

```
hse_send(desc: &Desc)
    │
    ├─▶ spin until !channel_busy()    [FSR/TSR/RSR check, timeout → Err(Raw(0))]
    │
    ├─▶ compiler_fence(Release)       [ensure all descriptor writes reach SRAM]
    │
    ├─▶ write_volatile(MU_TR1, &desc as u32)   [trigger HSE]
    │
    ├─▶ spin until RSR bit[1] set     [wait for response, timeout → Err(Raw(0))]
    │
    ├─▶ resp = read_volatile(MU_RR1)  [read hseSrvResponse_t]
    │
    ├─▶ spin until FSR bit[1] clear   [wait for HSE fully done]
    │
    └─▶ match resp:
            HSE_SRV_RSP_OK            → Ok(())
            HSE_SRV_RSP_VERIFY_FAILED → Err(HseError::VerifyFailed)
            other                     → Err(HseError::Raw(other))
```

### Descriptor Layout

Every HSE service descriptor is a 256-byte, 4-byte-aligned buffer (`Desc`).
The first 8 bytes are the common header; bytes 8 onward are service-specific.

```
[0..4]   srvId          (u32, little-endian)
[4..8]   srvMetaData    (u32) = 0
[8..]    service struct (up to 248 bytes)
```

**AES-CBC (`hseSymCipherSrv_t`):**
```
[8]      accessMode     ONE_PASS = 0
[9]      streamId       0
[10]     cipherAlgo     AES = 0x10
[11]     cipherBlockMode CBC = 2
[12]     cipherDir      ENCRYPT = 1 / DECRYPT = 0
[13]     sgtOption      0
[14..15] reserved
[16..20] keyHandle      (u32)
[20..24] pIV            (u32 — pointer to 16-byte IV)
[24..28] inputLength    (u32 — must be multiple of 16)
[28..32] pInput         (u32)
[32..36] pOutput        (u32)
```

**AES-CMAC (`hseMacSrv_t`):**
```
[8]      accessMode     ONE_PASS = 0
[9]      streamId       0
[10]     authDir        GENERATE = 1 / VERIFY = 0
[11]     sgtOption      0
[12]     macAlgo        CMAC = 0x11
[13..15] reserved
[16]     cmac.cipherAlgo AES = 0x10
[17..23] reserved / union padding
[24..28] keyHandle      (u32)
[28..32] inputLength    (u32)
[32..36] pInput         (u32)
[36..40] pTagLength     (u32 — pointer to u32 with tag length = 16)
[40..44] pTag           (u32)
```

**HMAC (`hseMacSrv_t`) — same struct, different `[12]` and `[16]`:**
```
[8]      accessMode     ONE_PASS = 0
[9]      streamId       0
[10]     authDir        GENERATE = 1 / VERIFY = 0
[11]     sgtOption      0
[12]     macAlgo        HMAC = 0x20          ← differs from CMAC
[13..15] reserved
[16]     hmac.hashAlgo  e.g. SHA2_256 = 4   ← differs from CMAC
[17..23] reserved / union padding
[24..28] keyHandle      (u32 — must be an HMAC-type key slot)
[28..32] inputLength    (u32)
[32..36] pInput         (u32)
[36..40] pTagLength     (u32 — pointer to u32 holding tag buffer capacity;
                              HSE writes back actual tag length on GENERATE)
[40..44] pTag           (u32 — output on GENERATE, input on VERIFY)
```

`hseHashAlgo_t` values used by HMAC:

| Constant | Value | Output bytes |
|---|---|---|
| `HASH_ALGO_SHA_1` | 2 | 20 |
| `HASH_ALGO_SHA2_224` | 3 | 28 |
| `HASH_ALGO_SHA2_256` | 4 | 32 |
| `HASH_ALGO_SHA2_384` | 5 | 48 |
| `HASH_ALGO_SHA2_512` | 6 | 64 |

**RNG DRG4 (`hseGetRandomNumSrv_t`):**
```
[8]      rngClass       DRG4 = 1
[9..11]  reserved
[12..16] randomNumLength (u32)
[16..20] pRandomNum     (u32)
```

**Plain Key Import (`hseImportKeySrv_t`):**
```
[8..12]  targetKeyHandle         (u32)
[12..16] pKeyInfo                (u32 — pointer to 16-byte hseKeyInfo_t)
[16..24] pKey[0], pKey[1]        0 (RSA/ECC fields, unused for AES)
[24..28] pKey[2]                 (u32 — pointer to raw key bytes)
[28..34] keyLen[0], keyLen[1]    0
[32..34] keyLen[2]               (u16 — key byte length)
[34..36] reserved
[36..40] cipher.cipherKeyHandle  0xFFFF_FFFF (plain import)
[40..64] cipher.cipherScheme     0 (24 bytes)
[72..76] keyContainer.authKeyHandle  0xFFFF_FFFF (no auth)
[76..88] keyContainer.authScheme 0 (12 bytes)
[100..104] keyFormat             0
```

`hseKeyInfo_t` (16-byte stack struct built inline):
```
[0..2]   keyFlags       (u16)  — e.g. ENCRYPT|DECRYPT = 0x0003
[2..4]   keyBitLen      (u16)  — e.g. 128
[4..8]   keyCounter     (u32)  = 0 for RAM keys
[8..12]  smrFlags       (u32)  = 0
[12]     keyType        (u8)   = AES = 0x12  or  HMAC = 0x20
[13]     specific (u8)  — aesBlockModeMask for AES (0 = any mode, bit[2] = CBC-only)
                       — 0 for HMAC (field unused)
[14..16] reserved
```

### Public API

```rust
// ── AES cipher ────────────────────────────────────────────────────────────

/// Fill `output` with AES-CBC-128 ciphertext.
pub fn aes_cbc_encrypt(
    key_handle: u32,
    iv: &[u8; 16],
    input: &[u8],
    output: &mut [u8],
) -> Result<(), HseError>

/// Decrypt AES-CBC-128 ciphertext.
pub fn aes_cbc_decrypt(
    key_handle: u32,
    iv: &[u8; 16],
    input: &[u8],
    output: &mut [u8],
) -> Result<(), HseError>

// ── AES-CMAC ──────────────────────────────────────────────────────────────

/// Generate a 16-byte AES-CMAC tag.
pub fn aes_cmac_generate(
    key_handle: u32,
    input: &[u8],
    tag: &mut [u8; 16],
) -> Result<(), HseError>

/// Verify a 16-byte AES-CMAC tag.
/// Returns `Err(HseError::VerifyFailed)` on mismatch.
pub fn aes_cmac_verify(
    key_handle: u32,
    input: &[u8],
    tag: &[u8; 16],
) -> Result<(), HseError>

// ── HMAC ──────────────────────────────────────────────────────────────────

/// Generate an HMAC tag over `input`.
/// `tag` must be at least `hash_algo.tag_len()` bytes.
pub fn hmac_generate(
    key_handle: u32,
    hash_algo: HseHashAlgo,
    input: &[u8],
    tag: &mut [u8],
) -> Result<(), HseError>

/// Verify an HMAC tag.
/// Returns `Err(HseError::VerifyFailed)` on mismatch.
pub fn hmac_verify(
    key_handle: u32,
    hash_algo: HseHashAlgo,
    input: &[u8],
    tag: &[u8],
) -> Result<(), HseError>

// ── RNG ───────────────────────────────────────────────────────────────────

/// Fill `output` with DRG4 random bytes.
pub fn get_rng_drg4(output: &mut [u8]) -> Result<(), HseError>

// ── Key import ────────────────────────────────────────────────────────────

/// Import a plain AES key into an HSE RAM or NVM slot.
pub fn import_plain_sym_key(
    target_key_handle: u32,
    key_flags: u16,          // combine key_flags::* constants
    key_bit_len: u16,        // e.g. 128
    aes_block_mode_mask: u8, // 0 = all modes, 1<<2 = CBC-only
    key_bytes: &[u8],
) -> Result<(), HseError>

/// Import a plain HMAC key into an HSE RAM or NVM slot.
/// Sets keyType = HSE_KEY_TYPE_HMAC (0x20); the slot can only
/// be used with hmac_generate / hmac_verify.
pub fn import_plain_hmac_key(
    target_key_handle: u32,
    key_flags: u16,    // typically key_flags::SIGN_VERIFY
    key_bit_len: u16,  // e.g. 256 for a 32-byte key
    key_bytes: &[u8],
) -> Result<(), HseError>
```

`key_flags` constants (shared by AES and HMAC keys):

```rust
key_flags::ENCRYPT          // 0x0001  AES only
key_flags::DECRYPT          // 0x0002  AES only
key_flags::SIGN             // 0x0004  CMAC/HMAC generate
key_flags::VERIFY           // 0x0008  CMAC/HMAC verify
key_flags::ENCRYPT_DECRYPT  // 0x0003
key_flags::SIGN_VERIFY      // 0x000C
```

`HseHashAlgo` enum (used by HMAC functions):

```rust
pub enum HseHashAlgo {
    Sha1     = 2,  // 20 bytes
    Sha2_224 = 3,  // 28 bytes
    Sha2_256 = 4,  // 32 bytes  ← recommended
    Sha2_384 = 5,  // 48 bytes
    Sha2_512 = 6,  // 64 bytes
}
// HseHashAlgo::tag_len(self) -> usize — output length in bytes
// HseHashAlgo::from_u8(u8)   -> Option<Self> — from raw hseHashAlgo_t value
```

### Error Type

```rust
pub enum HseError {
    /// HSE_SRV_RSP_VERIFY_FAILED — CMAC or HMAC tag did not match.
    VerifyFailed,
    /// Any other non-OK response, or MU channel timeout (Raw(0)).
    Raw(u32),
}
```

### Safety Notes

* All nine public functions are **safe** Rust.  The only `unsafe` code is the
  volatile MMIO register access inside `mu_r` / `mu_w`.
* Descriptor addresses passed to `MU_TR1` are physical addresses of stack
  variables.  They are valid because `hse_send` is fully synchronous — it
  blocks until HSE completes before returning.
* `tag_len` (CMAC / HMAC generate), `tag_len` (CMAC / HMAC verify), and
  `KeyInfo` (both import functions) are kept alive on the stack using a
  `read_volatile(addr_of!(...))` fence after `hse_send` returns, preventing
  the compiler from reusing the stack slot prematurely.
* HSE must be fully initialised (key catalog formatted, `HSE_STATUS_INSTALL_OK`
  set) before calling any function in this module.

---

## `communication_middleware.rs`

`rust/src/communication_middleware.rs` provides a blocking UART transport
layer and, on top of it, a symmetric-key secure channel.  UART I/O is
delegated to the NXP C RTD drivers via `ffi.rs`; all cryptographic operations
are delegated to `hse.rs`.

### Architecture

```
┌──────────────────────────────────────────────────────────────┐
│           communication_middleware.rs (Rust)                  │
│                                                               │
│  C-callable exports (hse_rust.h):                            │
│  secure_comm_init / secure_comm_init_she_keys                 │
│  secure_send_to_ap_encrypted / secure_recv_from_ap_encrypted  │
│  send_to_ap / recv_from_ap / send_to_ap2 / recv_from_ap2      │
│  req_save_data / req_saved_data                               │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐    │
│  │                   Secure Channel                      │    │
│  │                                                       │    │
│  │  State: CIPHER_KEY_HANDLE, MAC_KEY_HANDLE (AtomicU32) │    │
│  │                                                       │    │
│  │  secure_init()                                        │    │
│  │    ├─▶ catalog check/format  ─────────────────────────────▶ ffi.rs │
│  │    └─▶ hse::import_plain_sym_key() ×2 ───────────────────▶ hse.rs │
│  │                                                       │    │
│  │  secure_send()                                        │    │
│  │    ├─▶ hse::get_rng_drg4()      (IV)  ───────────────────▶ hse.rs │
│  │    ├─▶ hse::aes_cbc_encrypt()         ───────────────────▶ hse.rs │
│  │    ├─▶ hse::aes_cmac_generate() (tag) ───────────────────▶ hse.rs │
│  │    └─▶ send_data()              ─────────────────────────▶ UART  │
│  │                                                       │    │
│  │  secure_receive()                                     │    │
│  │    ├─▶ receive_data()           ─────────────────────────▶ UART  │
│  │    ├─▶ hse::aes_cmac_verify()   (tag) ───────────────────▶ hse.rs │
│  │    └─▶ hse::aes_cbc_decrypt()         ───────────────────▶ hse.rs │
│  └──────────────────────────────────────────────────────┘    │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐    │
│  │                   Plain Transport                     │    │
│  │                                                       │    │
│  │  send_data() / receive_data()                         │    │
│  │    UartBackend::Lpuart   ──────────────────────────────────▶ ffi.rs │
│  │    UartBackend::FlexioTx ──────────────────────────────────▶ ffi.rs │
│  │    UartBackend::FlexioRx ──────────────────────────────────▶ ffi.rs │
│  └──────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────┘
         │ unsafe FFI (ffi.rs)              │ hse::* (safe)
┌────────┴──────────────────┐   ┌──────────┴───────────────────┐
│  C RTD UART + catalog FFI │   │   hse.rs (MMIO driver)        │
│  Lpuart_Uart_Ip_*         │   │   → MU0 registers             │
│  Flexio_Uart_Ip_*         │   │   → HSE_B firmware            │
│  FormatKeyCatalogs_()     │   └──────────────────────────────┘
│  ParseKeyCatalogs()       │
└───────────────────────────┘
```

### Hardware Channel Assignments

| Constant            | Value | Role                        |
|---------------------|-------|-----------------------------|
| `LPUART_CHANNEL`    | 3     | LPUART instance 3           |
| `FLEXIO_TX_CHANNEL` | 0     | FlexIO UART transmit ch. 0  |
| `FLEXIO_RX_CHANNEL` | 1     | FlexIO UART receive ch. 1   |

### HSE Key Slot Assignments

Keys live in the **RAM catalog group 1** (10 × AES-128 slots).

| Purpose       | Handle (GET_KEY_HANDLE) | Slot    | Usage flags              |
|---------------|-------------------------|---------|--------------------------|
| CBC cipher    | `0x020100` (RAM, 1, 0)  | slot 2  | ENCRYPT \| DECRYPT, CBC only |
| CMAC          | `0x020101` (RAM, 1, 1)  | slot 3  | SIGN \| VERIFY           |

> **Note:** Slots 0 and 1 in group 1 (`HSE_DEMO_RAM_AES128_KEY0/1`) are used by the existing HSE crypto examples for AES-ECB and AES-GMAC. Use slots 2+ to avoid conflicts.

---

### Secure Channel Initialisation Flow

```
secure_comm_init_she_keys()
    │
    ├─▶ IsKeyCatalogFormatted()   [FFI — catalog check]
    │       ├─▶ NOT formatted
    │       │       └─▶ FormatKeyCatalogs_()  [FFI]        ──fail──▶ return false
    │       │               (formats NVM + RAM catalogs,
    │       │                calls ParseKeyCatalogs() internally)
    │       └─▶ already formatted
    │               └─▶ ParseKeyCatalogs()  [FFI]          ──fail──▶ return false
    │                       (re-initialises HKF allocator,
    │                        NVM keys preserved)
    │
    ├─▶ hse::import_plain_sym_key(RAM group-1, slot 0,    ──fail──▶ return false
    │       key_flags::ENCRYPT_DECRYPT, 128 bits,
    │       aes_block_mode_mask = CBC-only, SHE CBC key)
    │       └─▶ store handle → CIPHER_KEY_HANDLE
    │
    ├─▶ hse::import_plain_sym_key(RAM group-1, slot 1,    ──fail──▶ return false
    │       key_flags::SIGN_VERIFY, 128 bits,
    │       aes_block_mode_mask = 0 (any), SHE CMAC key)
    │       └─▶ store handle → MAC_KEY_HANDLE
    │
    └─▶ return true
```

---

### Secure Send Flow (`secure_send_to_ap_encrypted`)

```
secure_send_to_ap_encrypted(data, len)
    │
    ├─▶ validate: data != NULL, 0 < len ≤ 224, len % 16 == 0    ──▶ return false
    │
    ├─▶ validate: CIPHER_KEY_HANDLE and MAC_KEY_HANDLE set        ──▶ return false
    │
    ├─▶ hse::get_rng_drg4(&mut frame[0..16])                     ──fail──▶ return false
    │       └─▶ fill frame[0..16] with fresh random IV (DRG4)
    │
    ├─▶ hse::aes_cbc_encrypt(cipher_handle,                      ──fail──▶ return false
    │       iv = frame[0..16], input = data → frame[16..16+len])
    │       └─▶ encrypt plaintext into frame
    │
    ├─▶ hse::aes_cmac_generate(mac_handle,                       ──fail──▶ return false
    │       input = frame[0..16+len], tag → frame[16+len..32+len])
    │       └─▶ MAC over IV ‖ ciphertext, tag appended
    │
    ├─▶ send_data(backend, frame[0..32+len])                     ──fail──▶ return false
    │       └─▶ transmit complete frame over UART
    │
    └─▶ return true

    On-wire frame:
    ┌──────────────┬────────────────────────┬──────────────┐
    │   IV  (16 B) │   Ciphertext  (len B)  │  CMAC (16 B) │
    └──────────────┴────────────────────────┴──────────────┘
                          total = len + 32 bytes  (max 256 B)
```

---

### Secure Receive Flow (`secure_recv_from_ap_encrypted`)

```
secure_recv_from_ap_encrypted(buf, expected_ct_len, out_len)
    │
    ├─▶ validate: buf != NULL, out_len != NULL                    ──▶ return false
    │             0 < expected_ct_len ≤ 224, % 16 == 0
    │
    ├─▶ validate: CIPHER_KEY_HANDLE and MAC_KEY_HANDLE set        ──▶ return false
    │
    ├─▶ receive_data(backend, frame[0..expected_ct_len+32])       ──fail──▶ return false
    │       └─▶ receive complete frame from UART
    │
    ├─▶ hse::aes_cmac_verify(mac_handle,                         ──▶ MacVerifyFailed
    │       input = frame[0..16+ct_len],                                  → return false
    │       tag   = frame[16+ct_len..32+ct_len])
    │       │   verify CMAC over IV ‖ ciphertext BEFORE decrypting
    │       └─▶ on HseError::VerifyFailed  ──▶ return false
    │
    ├─▶ hse::aes_cbc_decrypt(cipher_handle,                      ──fail──▶ return false
    │       iv  = frame[0..16],
    │       ct  = frame[16..16+ct_len] → buf)
    │       └─▶ decrypt verified ciphertext
    │
    ├─▶ *out_len = expected_ct_len
    │
    └─▶ return true
```

---

### C API Reference

All functions are declared in `include/hse_rust.h`.

#### Initialisation

```c
/* Initialise secure channel with built-in SHE example keys.
   Handles catalog formatting and key provisioning automatically.
   Call once before any encrypted send/receive. */
bool secure_comm_init_she_keys(void);

/* Initialise with caller-supplied 16-byte keys. */
bool secure_comm_init(const uint8_t *cipher_key, const uint8_t *mac_key);
```

#### Secure (encrypted) transfer

```c
/* Encrypt and send len bytes of plaintext over LPUART.
   len must be a non-zero multiple of 16, max 224 bytes. */
bool secure_send_to_ap_encrypted(const uint8_t *data, uint32_t len);

/* Receive, verify, and decrypt a secure frame from LPUART.
   expected_ct_len == plaintext length used by the sender.
   On success *out_len == expected_ct_len. */
bool secure_recv_from_ap_encrypted(uint8_t *buf,
                                   uint32_t expected_ct_len,
                                   uint32_t *out_len);
```

#### Plain (unencrypted) transport

```c
bool send_to_ap(const uint8_t *data, uint32_t len);           /* LPUART TX */
bool send_to_ap2(const uint8_t *data, uint32_t len,
                 bool use_flexio);                            /* LPUART or FlexIO TX */
bool recv_from_ap(uint8_t *buf, uint32_t len);                /* LPUART RX */
bool recv_from_ap2(uint8_t *buf, uint32_t len,
                   bool use_flexio);                          /* LPUART or FlexIO RX */
bool req_save_data(const uint8_t *data, uint32_t len);        /* send over LPUART */
bool req_saved_data(uint8_t *buf, uint32_t len);              /* receive over LPUART */
```

---

### Security Properties

| Property           | Mechanism                                        |
|--------------------|--------------------------------------------------|
| Confidentiality    | AES-CBC-128, fresh random IV per frame (DRG4 RNG)|
| Integrity + Auth   | AES-CMAC-128 over full IV ‖ ciphertext           |
| Order of ops       | Encrypt-then-MAC; MAC verified before decryption |
| Key storage        | HSE RAM key slots (volatile, lost on power cycle)|
| Max frame size     | 256 bytes (224 B plaintext + 32 B overhead)      |

---

**This is the actual production code from NXP S32K344 Template - ready to use in your projects!**

**License**: BSD 3-Clause (NXP)  
**Source**: S32K344_DemoAppTemplate  
**Status**: Production-Ready
