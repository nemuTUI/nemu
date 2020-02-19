macro (check_glibc_version)
    execute_process (
        COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.so.6
    OUTPUT_VARIABLE GLIBC
    OUTPUT_STRIP_TRAILING_WHITESPACE)

    get_filename_component (GLIBC ${GLIBC} REALPATH)
    get_filename_component (GLIBC_VERSION ${GLIBC} NAME)
    string (REPLACE "libc-" "" GLIBC_VERSION ${GLIBC_VERSION})
    string (REPLACE ".so" "" GLIBC_VERSION ${GLIBC_VERSION})
    if (NOT GLIBC_VERSION MATCHES "^[0-9.]+$")
        unset (GLIBC_VERSION)
        message (FATAL_ERROR "Unknown glibc version: ${GLIBC_VERSION}")
    endif (NOT GLIBC_VERSION MATCHES "^[0-9.]+$")

endmacro(check_glibc_version)
