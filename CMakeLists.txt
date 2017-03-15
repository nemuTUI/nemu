cmake_minimum_required(VERSION 2.8)
project(nemu)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(Procps REQUIRED)
find_package(Threads REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(UDev REQUIRED)

#include_directories(${CURSES_INCLUDE_DIR}
#                    ${SQLITE3_INCLUDE_DIR}
#                    ${PROCPS_INCLUDE_DIRS}
#                    "src")
include_directories("src")

aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}
    ${CURSES_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${PROCPS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBUSB_1_LIBRARIES}
    ${UDEV_LIBRARIES})

set(USR_PREFIX "/usr" CACHE PATH "Executable and lang files directory prefix")
add_definitions(-DNM_USR_PREFIX=${USR_PREFIX})

add_subdirectory(lang)
add_dependencies(${PROJECT_NAME} lang)

include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG("-std=gnu99" COMPILER_SUPPORTS_GNU99)
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS
    "-Wall -Wextra -pedantic -isystem ${LIBUSB_1_INCLUDE_DIRS} ")

if(COMPILER_SUPPORTS_GNU99)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=gnu99")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_C_COMPILER} has no C99 support. Please use a different C compiler.")
endif()
