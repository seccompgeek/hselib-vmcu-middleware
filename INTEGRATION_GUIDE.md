# Using the HSE Library with Your Projects

This guide shows how to integrate the HSE Library (extracted from the S32K344 Template) with other projects in the demo suite.

## Quick Integration

### Step 1: Copy Library to Your Project

```bash
cp -r HSELib /path/to/S32K344_AES_EncryptDecrypt/
cd /path/to/S32K344_AES_EncryptDecrypt
```

### Step 2: Add to S32DS Project

> **Note:** the library includes a minimal `sys_init.h` stub under
> `include/` because the host code refers to a startup file in the Template
> project.  In a real application you can replace it with your own system
> initialization header or simply ignore it if not used.

In S32DS IDE:

1. Right-click on your project → **Properties**
2. **C/C++ Build** → **Settings**
3. **Include paths** – add the following directories (HSE headers are spread
   across them):
   ```
   ${PROJECT_LOC}/HSELib/lib
   ${PROJECT_LOC}/HSELib/lib/host_hse
   ${PROJECT_LOC}/HSELib/lib/host_keymgmt
   ${PROJECT_LOC}/HSELib/lib/host_crypto_helper
   ${PROJECT_LOC}/HSELib/lib/services/inc  # contains both service-definition headers (hse_srv_*.h) and example helpers (global_defs.h, hse_monotonic_cnt.h, etc.)
   ${PROJECT_LOC}/HSELib/drivers
   ${PROJECT_LOC}/HSELib/interface
   ${PROJECT_LOC}/HSELib/include
   ```
4. **Source locations** – either add individual folders or simply reference the
   C files you need. A convenient approach is to add the following globs:
   ```
   ${PROJECT_LOC}/HSELib/lib/host_hse/*.c
   ${PROJECT_LOC}/HSELib/lib/host_keymgmt/*.c
   ${PROJECT_LOC}/HSELib/lib/host_crypto_helper/*.c
   ${PROJECT_LOC}/HSELib/lib/services/src/fw_crypto/*.c
   ${PROJECT_LOC}/HSELib/lib/services/src/common/*.c
   ${PROJECT_LOC}/HSELib/drivers/mu/*.c
   ${PROJECT_LOC}/HSELib/drivers/stm/*.c
   ```

### Step 3: Add to CMake or Makefile

*(These snippets were previously later in the document.)*

## CMake Integration

The library comes with its own `CMakeLists.txt` that builds a static
archive (`libhselib.a`).  You can directly add the subdirectory and link
against the provided `hselib` target:

```cmake
set(HSELIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/HSELib)
add_subdirectory(${HSELIB_DIR} hselib_build)

add_executable(myapp main.c ...)
target_link_libraries(myapp PRIVATE hselib)
```

That CMake file already configures all include paths and selects the
relevant drivers and host sources; the only files it skips are the
example routines under `lib/services/src/fw_crypto` because they contain
application‑specific initializers which do not compile as part of a
standalone library.  Add them manually if you want the demos.

Alternatively, if you prefer to build the sources directly into your
project rather than linking an external library, you can use this
pattern (again omitting the `fw_crypto` files unless needed):

```cmake
include_directories(
    ${HSELIB_DIR}/lib/host_hse
    ${HSELIB_DIR}/lib/host_keymgmt
    ${HSELIB_DIR}/lib/host_crypto_helper
    ${HSELIB_DIR}/lib/services/inc
    ${HSELIB_DIR}/drivers
    ${HSELIB_DIR}/interface
    ${HSELIB_DIR}/include
)

file(GLOB_RECURSE HSE_SOURCES
    "${HSELIB_DIR}/lib/host_hse/*.c"
    "${HSELIB_DIR}/lib/host_keymgmt/*.c"
    "${HSELIB_DIR}/lib/host_crypto_helper/*.c"
    #"${HSELIB_DIR}/lib/services/src/fw_crypto/*.c"  # optional demo code
    "${HSELIB_DIR}/lib/services/src/common/*.c"
    "${HSELIB_DIR}/drivers/mu/*.c"
    "${HSELIB_DIR}/drivers/stm/*.c"
)

target_sources(${PROJECT_NAME} PRIVATE ${HSE_SOURCES})
```

## Makefile Integration

```makefile
# Path to HSELib
HSELIB = HSELib

# Add to compiler include paths
CFLAGS += -I${HSELIB}/lib \
          -I${HSELIB}/lib/host_hse \
          -I${HSELIB}/lib/host_keymgmt \
          -I${HSELIB}/lib/host_crypto_helper \
          -I${HSELIB}/lib/services/inc \
          -I${HSELIB}/drivers \
          -I${HSELIB}/interface \
          -I${HSELIB}/include

# Source files to compile
SOURCES += ${HSELIB}/lib/host_hse/hse_host.c \
           ${HSELIB}/lib/host_keymgmt/*.c \
           ${HSELIB}/lib/host_crypto_helper/*.c \
           ${HSELIB}/drivers/mu/*.c \
           ${HSELIB}/drivers/stm/*.c

# For crypto services
SOURCES +=${HSELIB}/lib/services/src/fw_crypto/hse_crypto.c
```

## Using Specific Services

### Example A: Replace Direct Service Calls with Library

**Before (Direct implementation):**

```c
// From S32K344_AES_EncryptDecrypt/src/main.c
// Was defining and calling HSE services directly
```

... (the rest of the document continues unchanged) ...

## Using Specific Services

### Example A: Replace Direct Service Calls with Library

**Before (Direct implementation):**

```c
// From S32K344_AES_EncryptDecrypt/src/main.c
// Was defining and calling HSE services directly
```

**After (Using HSELib):**

```c
#include "hse_host.h"
#include "hse_host_cipher.h"

int main(void) {
    // HSE is already initialized by Template code
    
    // Get free channel
    uint8_t channel = HSE_GetFreeChannel(MU0);
    
    // Get service descriptor
    hseSrvDescriptor_t* pHseSrvDesc = gHseSrvDesc[MU0];
    
    // Configure and send service - see hse_crypto.c examples
    // ...
    
    return 0;
}
```

### Example B: AES Encryption

From the pattern in `lib/services/fw_crypto/hse_crypto.c`:

```c
#include "hse_host.h"
#include "hse_host_cipher.h"

hseSrvResponse_t status;
uint8_t plaintext[16] = {...};
uint8_t ciphertext[16];
uint8_t key[32] = {...};

uint8_t channel = HSE_GetFreeChannel(MU0);
hseSrvDescriptor_t* desc = gHseSrvDesc[MU0];

desc->srvId = HSE_SRV_CIPHER;
desc->srvPayload.cipherReq.cipherAlgo = HSE_CIPHER_ALGO_AES;
desc->srvPayload.cipherReq.cipherMode = HSE_CIPHER_MODE_ECB;
desc->srvPayload.cipherReq.cipherDir = HSE_CIPHER_DIR_ENCRYPT;
desc->srvPayload.cipherReq.inputLength = 16;
desc->srvPayload.cipherReq.pIn = PTR_TO_HOST_ADDR(plaintext);
desc->srvPayload.cipherReq.pOut = PTR_TO_HOST_ADDR(ciphertext);
desc->srvPayload.cipherReq.cipherKeyHandle = PTR_TO_HOST_ADDR(key);

status = HSE_Send(MU0, channel, gSyncTxOption, desc);
```

### Example C: SHA-256 Hash

```c
#include "hse_host.h"

hseSrvResponse_t status;
uint8_t message[] = "test message";
uint8_t hash[32];

uint8_t channel = HSE_GetFreeChannel(MU0);
hseSrvDescriptor_t* desc = gHseSrvDesc[MU0];

desc->srvId = HSE_SRV_HASH;
desc->srvPayload.hashReq.hashAlgo = HSE_HASH_ALGO_SHA2_256;
desc->srvPayload.hashReq.inputLength = sizeof(message) - 1;
desc->srvPayload.hashReq.pIn = PTR_TO_HOST_ADDR(message);
desc->srvPayload.hashReq.pHashLength = PTR_TO_HOST_ADDR(hash);

status = HSE_Send(MU0, channel, gSyncTxOption, desc);
```

### Example D: CMAC Generation

```c
#include "hse_host.h"
#include "hse_host_mac.h"

hseSrvResponse_t status;
uint8_t message[] = "message";
uint8_t key[16] = {...};
uint8_t tag[16];

uint8_t channel = HSE_GetFreeChannel(MU0);
hseSrvDescriptor_t* desc = gHseSrvDesc[MU0];

desc->srvId = HSE_SRV_MAC;
desc->srvPayload.macReq.macAlgo = HSE_MAC_ALGO_CMAC;
desc->srvPayload.macReq.macKeyHandle = PTR_TO_HOST_ADDR(key);
desc->srvPayload.macReq.inputLength = sizeof(message) - 1;
desc->srvPayload.macReq.pIn = PTR_TO_HOST_ADDR(message);
desc->srvPayload.macReq.pTag = PTR_TO_HOST_ADDR(tag);
desc->srvPayload.macReq.tagLength = sizeof(tag);

status = HSE_Send(MU0, channel, gSyncTxOption, desc);
```

## Which Projects to Use With

| Project | Library Fit | Example |
|---------|----------|---------|
| S32K344_AES_EncryptDecrypt | ✅ Perfect | Replace main.c service setup |
| S32K344_CMAC_GenVer | ✅ Perfect | Use MAC service directly |
| S32K344_Hse_ECC_Example | ✅ Good | Use sign/verify from lib/host_hse |
| S32K344_Basic_SecureBoot | ✅ Good | Use hash and sign services |
| S32K344_Advanced_SecureBoot | ✅ Good | Full service support |
| S32K344_DemoApp_SessionKeys | ✅ Good | Key management support |

## CMake Integration

The library now ships with its own build script; either link against the
pre‑built archive or compile the sources directly:

```cmake
set(HSELIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/HSELib)
add_subdirectory(${HSELIB_DIR} hselib_build)

add_executable(myapp main.c ...)
target_link_libraries(myapp PRIVATE hselib)
```

If you prefer to pull in the sources yourself, use the following snippet
and omit the `fw_crypto` examples unless you intend to build them:

```cmake
include_directories(
    ${HSELIB_DIR}/lib/host_hse
    ${HSELIB_DIR}/lib/host_keymgmt
    ${HSELIB_DIR}/lib/host_crypto_helper
    ${HSELIB_DIR}/lib/services/inc
    ${HSELIB_DIR}/drivers
    ${HSELIB_DIR}/interface
    ${HSELIB_DIR}/include
)

file(GLOB_RECURSE HSE_SOURCES
    "${HSELIB_DIR}/lib/host_hse/*.c"
    "${HSELIB_DIR}/lib/host_keymgmt/*.c"
    "${HSELIB_DIR}/lib/host_crypto_helper/*.c"
    #"${HSELIB_DIR}/lib/services/src/fw_crypto/*.c"  # optional demo code
    "${HSELIB_DIR}/lib/services/src/common/*.c"
    "${HSELIB_DIR}/drivers/mu/*.c"
    "${HSELIB_DIR}/drivers/stm/*.c"
)

target_sources(${PROJECT_NAME} PRIVATE ${HSE_SOURCES})
```

## Makefile Integration

```makefile
# Path to HSELib
HSELIB = HSELib

# Add to compiler include paths
CFLAGS += -I${HSELIB}/lib \
          -I${HSELIB}/lib/host_hse \
          -I${HSELIB}/lib/host_keymgmt \
          -I${HSELIB}/lib/host_crypto_helper \
          -I${HSELIB}/lib/services/inc \
          -I${HSELIB}/drivers \
          -I${HSELIB}/interface \
          -I${HSELIB}/include

# Source files to compile
SOURCES += ${HSELIB}/lib/host_hse/hse_host.c \
           ${HSELIB}/lib/host_keymgmt/*.c \
           ${HSELIB}/lib/host_crypto_helper/*.c \
           ${HSELIB}/drivers/mu/*.c \
           ${HSELIB}/drivers/stm/*.c

# For crypto services
SOURCES +=${HSELIB}/lib/services/src/fw_crypto/hse_crypto.c
```

## Reference Implementation

The best reference is `lib/services/fw_crypto/hse_crypto.c` which contains:
- Complete examples for all operations
- Proper error handling
- Test vectors
- async/sync patterns

Simply copy patterns from there into your application.

## Key Functions to Use

From `hse_host.h`:
```c
// Send HSE service request
hseSrvResponse_t HSE_Send(uint8_t mu_instance, uint8_t channel, 
                          hseTxOptions_t options, 
                          hseSrvDescriptor_t* descriptor);

// Get free channel
uint8_t HSE_GetFreeChannel(uint8_t mu_instance);

// Global structures
extern hseSrvDescriptor_t* const gHseSrvDesc[HSE_NUM_OF_MU_INSTANCES];
extern const hseTxOptions_t gSyncTxOption;
```

## Testing

To test the library:

1. Copy `test/src/hse_library_example.c` to your project
2. Compile and link with HSELib includes and sources
3. Run the example - demonstrates AES, SHA-256, CMAC

## Common Issues

**Missing includes**: Make sure all HSELib paths are in project include paths

**Link errors**: Add all HSE source files to the build

**Runtime errors**: Verify HSE firmware is initialized before calling services

## Next Steps

1. Copy HSELib to your project
2. Add include and source paths to build configuration
3. Include necessary headers in your code
4. Follow patterns from `lib/services/fw_crypto/hse_crypto.c`
5. Build and test

---

**This is the actual production code - no wrappers or simplifications, just direct HSE service interface!**
