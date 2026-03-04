# cmake/MergeRustArchive.cmake
#
# Merges RUST_ARCHIVE into OUTPUT_ARCHIVE using an ar MRI script.
# Invoked from the hselib POST_BUILD step in CMakeLists.txt.
#
# Required variables (passed via -D):
#   OUTPUT_ARCHIVE   absolute path to libhselib.a
#   RUST_ARCHIVE     absolute path to libhselib_rust.a
#   AR               arm-none-eabi-ar (or equivalent)
#   RANLIB           arm-none-eabi-ranlib (or equivalent)
cmake_minimum_required(VERSION 3.15)

foreach(_var OUTPUT_ARCHIVE RUST_ARCHIVE AR RANLIB)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "MergeRustArchive.cmake: variable ${_var} not set")
    endif()
endforeach()

# Write a temporary ar MRI script next to the output archive.
set(_mri "${OUTPUT_ARCHIVE}.mri")
file(WRITE "${_mri}"
    "OPEN ${OUTPUT_ARCHIVE}\nADDLIB ${RUST_ARCHIVE}\nSAVE\nEND\n")

execute_process(
    COMMAND "${AR}" -M
    INPUT_FILE "${_mri}"
    RESULT_VARIABLE _res
)
file(REMOVE "${_mri}")

if(_res)
    message(FATAL_ERROR "ar merge failed (exit ${_res})")
endif()

execute_process(
    COMMAND "${RANLIB}" "${OUTPUT_ARCHIVE}"
    RESULT_VARIABLE _res2
)
if(_res2)
    message(FATAL_ERROR "ranlib failed (exit ${_res2})")
endif()

message(STATUS "Rust archive merged into ${OUTPUT_ARCHIVE}")
