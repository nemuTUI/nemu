cmake_minimum_required(VERSION 3.10)
project(libspice VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_C_FLAGS "-Wall -Wextra -Wformat-security -pedantic")

find_package(PkgConfig REQUIRED)

pkg_check_modules(SPICE_PROTOCOL REQUIRED spice-protocol)

find_package(X11 REQUIRED)

find_path(CRYPTO_INCLUDE_DIR openssl/crypto.h)
find_library(CRYPTO_LIB crypto)
if(NOT CRYPTO_LIB OR NOT CRYPTO_INCLUDE_DIR)
    message(FATAL_ERROR "libcrypto not found. Install openssl-devel or libssl-dev")
endif()

find_package(PNG REQUIRED)
if(NOT PNG_FOUND)
    find_library(PNG_LIB png)
    find_path(PNG_INCLUDE_DIR png.h)
    if(NOT PNG_LIB OR NOT PNG_INCLUDE_DIR)
        message(FATAL_ERROR "libpng is not found. Install libpng-devel or libpng-dev")
    endif()
endif()

message(STATUS "Found spice-protocol: ${SPICE_PROTOCOL_VERSION}")
message(STATUS "Found X11: ${X11_FOUND}")
message(STATUS "Found crypto: ${CRYPTO_LIB}")
message(STATUS "Found png: ${PNG_FOUND}")

add_library(spice STATIC
    sp_proto.c
    sp_utils.c
    sp_screen.c
)

set_target_properties(spice PROPERTIES
    OUTPUT_NAME "spice"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

target_link_libraries(spice
    m
    X11::X11
    ${CRYPTO_LIB}
    PNG::PNG
    ${SPICE_PROTOCOL_LIBRARIES}
)

target_compile_options(spice PRIVATE ${SPICE_PROTOCOL_CFLAGS_OTHER})
target_include_directories(spice SYSTEM PRIVATE
    ${SPICE_PROTOCOL_INCLUDE_DIRS}
    ${CRYPTO_INCLUDE_DIR}
)

target_include_directories(spice PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

message(STATUS "Building static library: libspice.a")
