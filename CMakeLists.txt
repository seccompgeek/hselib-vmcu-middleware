cmake_minimum_required(VERSION 3.15)

# Required for bare-metal targets: prevents CMake from trying to link a test
# executable (which fails without a C runtime providing _exit etc.)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(HSELib C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# Single flat include directory.
# In S32DS add HSELib/include to your include path (after SDK/S32DS paths so
# that S32DS platform headers take precedence).  Include guards make every
# shared NXP/CMSIS header safe to include from either location.
# =============================================================================
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# =============================================================================
# Single flat source directory.
# sys_init.c is intentionally NOT present in src/ — it defines
# SVC_Handler / PendSV_Handler / SysTick_Handler which conflict with FreeRTOS.
# nvic.c depends on AUTOSAR Os.h which is not available here; excluded.
# hse_crypto.c is included: it provides eccP256PubKey, eccP256PrivKey,
# hmacKeyInitial/Length, SetCustAuthorizationKey and Grant_SuperUser_Rights
# which are referenced directly by hse_config.c.
# Provide your own system initialisation in your application.
# =============================================================================
file(GLOB HSE_SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.c"
)

# Remove files that must NOT be part of the library
list(FILTER HSE_SOURCES EXCLUDE REGEX ".*/nvic\\.c$")

add_library(hselib STATIC ${HSE_SOURCES})

# Remap the build-machine's absolute source path to the canonical prefix
# "HSELib" inside all DWARF debug info.  This makes the library debuggable
# from any machine regardless of where the sources were checked out.
# On the debug machine, tell GDB:  set substitute-path HSELib /path/to/HSELib
target_compile_options(hselib PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -fdebug-prefix-map=${PROJECT_SOURCE_DIR}=HSELib
)

message(STATUS "HSELib sources (${CMAKE_CURRENT_LIST_FILE}):")
foreach(src ${HSE_SOURCES})
    message(STATUS "  ${src}")
endforeach()

# =============================================================================
# Rust component (rust/ subdirectory → libhselib_rust.a)
#
# Cargo is invoked as a custom build step.  The resulting static archive is
# merged into hselib so consumers see a single library.
#
# Target triple must match the C-side flags:
#   -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
# =============================================================================
find_program(CARGO_EXECUTABLE cargo
    HINTS "$ENV{HOME}/.cargo/bin" /usr/bin /usr/local/bin
    REQUIRED
)
message(STATUS "Cargo: ${CARGO_EXECUTABLE}")

set(RUST_CRATE_DIR     "${PROJECT_SOURCE_DIR}/rust")
set(RUST_TARGET_TRIPLE "thumbv7em-none-eabihf")
set(RUST_ARCHIVE_DIR   "${RUST_CRATE_DIR}/target/${RUST_TARGET_TRIPLE}/release")
set(RUST_ARCHIVE       "${RUST_ARCHIVE_DIR}/libhselib_rust.a")

# Re-run cargo whenever Rust sources or the manifest change.
file(GLOB_RECURSE RUST_SOURCES
    "${RUST_CRATE_DIR}/src/*.rs"
    "${RUST_CRATE_DIR}/Cargo.toml"
)

# Pass all rustflags via the environment so that CMake can inject the
# path-remap flags (which contain the build-machine-specific absolute path).
# This intentionally overrides any rustflags in .cargo/config.toml; the
# target-cpu flag is included here to compensate.
set(RUST_REMAP_FLAGS
    "--remap-path-prefix=${RUST_CRATE_DIR}=HSELib/rust"
    "--remap-path-prefix=$ENV{HOME}/.cargo=.cargo-home"
    "-C" "target-cpu=cortex-m7"
)
list(JOIN RUST_REMAP_FLAGS " " RUST_REMAP_FLAGS_STR)

add_custom_command(
    OUTPUT  "${RUST_ARCHIVE}"
    COMMAND "${CMAKE_COMMAND}" -E env
                "RUSTFLAGS=${RUST_REMAP_FLAGS_STR}"
            "${CARGO_EXECUTABLE}" build --release --target "${RUST_TARGET_TRIPLE}"
    WORKING_DIRECTORY "${RUST_CRATE_DIR}"
    DEPENDS ${RUST_SOURCES}
    COMMENT "Building Rust component: hselib_rust (${RUST_TARGET_TRIPLE})"
    VERBATIM
)

# Drive the Cargo build as part of the normal 'all' target.
add_custom_target(hselib_rust_cargo ALL DEPENDS "${RUST_ARCHIVE}")
add_dependencies(hselib hselib_rust_cargo)

# Merge the Rust archive into libhselib.a so consumers see one self-contained
# static library.  A cmake -P helper script writes a temporary ar MRI file to
# avoid shell-escaping issues that arise when using sh -c inside CMake.
add_custom_command(TARGET hselib POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        "-DOUTPUT_ARCHIVE=$<TARGET_FILE:hselib>"
        "-DRUST_ARCHIVE=${RUST_ARCHIVE}"
        "-DAR=${CMAKE_AR}"
        "-DRANLIB=${CMAKE_RANLIB}"
        -P "${PROJECT_SOURCE_DIR}/cmake/MergeRustArchive.cmake"
    COMMENT "Merging Rust archive into libhselib.a"
    VERBATIM
)
