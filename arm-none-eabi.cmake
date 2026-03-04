# Toolchain file for arm-none-eabi-gcc shipped with NXP S32DS 3.6
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_DIR "C:/NXP/S32DS.3.6.0/S32DS/build_tools/gcc_v11.4/gcc-11.4-arm32-eabi/bin")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_DIR}/arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/arm-none-eabi-g++.exe")
set(CMAKE_AR           "${TOOLCHAIN_DIR}/arm-none-eabi-ar.exe")
set(CMAKE_RANLIB       "${TOOLCHAIN_DIR}/arm-none-eabi-ranlib.exe")

# Bare-metal — no OS, generate Cortex-M7 code
set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")

# Don't try to link executables for try_compile checks
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Skip compiler ABI detection (no sysroot / runtime present)
set(CMAKE_C_COMPILER_FORCED   TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
