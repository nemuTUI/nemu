cmake_minimum_required(VERSION 3.13)
project(spice_client C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic")

add_subdirectory(../libspice libspice_build)

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
find_package(PNG REQUIRED)
find_library(CRYPTO_LIB crypto)

add_executable(spice_client spice_client.c)

target_include_directories(spice_client PRIVATE
    ../libspice/include)

target_link_libraries(spice_client PRIVATE
    m
    X11::X11
    crypto
    png
    spice
)
