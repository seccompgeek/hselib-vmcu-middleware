# Toolchain file for arm-none-eabi-gcc on Linux
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-linux.cmake .
#   cmake --build build
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Locate the toolchain from the system PATH
find_program(CMAKE_C_COMPILER   arm-none-eabi-gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++   REQUIRED)
find_program(CMAKE_AR           arm-none-eabi-ar    REQUIRED)
find_program(CMAKE_RANLIB       arm-none-eabi-ranlib REQUIRED)

# Bare-metal — Cortex-M7 with hardware FPU (matches S32K344 silicon)
set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")

# Don't try to link executables for try_compile checks (no sysroot available)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Skip compiler ABI detection (no runtime present)
set(CMAKE_C_COMPILER_FORCED   TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
