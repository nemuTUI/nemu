macro(get_glibc_version)
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.so.6
    OUTPUT_VARIABLE GLIBC
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(
    COMMAND ${GLIBC}
    COMMAND "head" "-n1"
    COMMAND "sed" "-r" "s|.*version ([0-9]*.[0-9]*).*|\\1|"
    OUTPUT_VARIABLE GLIBC_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT GLIBC_VERSION MATCHES "^[0-9.]+$")
    message(FATAL_ERROR "Unknown glibc version: ${GLIBC_VERSION}")
    unset(GLIBC_VERSION)
  endif(NOT GLIBC_VERSION MATCHES "^[0-9.]+$")

endmacro(get_glibc_version)
