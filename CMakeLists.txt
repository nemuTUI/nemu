cmake_minimum_required(VERSION 2.8)
project(nemu C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(Procps REQUIRED)
find_package(Threads REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(UDev REQUIRED)

include_directories(src)
aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}
    ${CURSES_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${PROCPS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBUSB_1_LIBRARIES}
    ${UDEV_LIBRARIES})

option(NM_DEBUG "Build debug version" OFF)
if (NM_DEBUG)
  add_definitions(-DNM_DEBUG)
endif()

set(USR_PREFIX "/usr" CACHE PATH "Executable and lang files directory prefix")
add_definitions(-DNM_USR_PREFIX=${USR_PREFIX})

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    add_definitions(-DNM_OS_LINUX)
elseif (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    message(FATAL_ERROR "Build on ${CMAKE_SYSTEM_NAME} will be supported later")
else()
    message(FATAL_ERROR "Build on ${CMAKE_SYSTEM_NAME} is not supported")
endif()

add_subdirectory(lang)
add_dependencies(${PROJECT_NAME} lang)

include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG("-std=c99" COMPILER_SUPPORTS_C99)
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS
    "-Wall -Wextra -pedantic -isystem ${LIBUSB_1_INCLUDE_DIRS} ")

if(COMPILER_SUPPORTS_C99)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c99")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_C_COMPILER} has no C99 support. Please use a different C compiler.")
endif()
