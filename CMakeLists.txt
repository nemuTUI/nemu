cmake_minimum_required(VERSION 2.8)
project(nemu C)
set(CMAKE_COLOR_MAKEFILE OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(UDev REQUIRED)
if (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
find_package(Intl REQUIRED)
endif()

include_directories(src)
aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})
if (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
target_link_libraries(${PROJECT_NAME}
    ${CURSES_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${PROCPS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBUSB_1_LIBRARIES}
    ${Intl_LIBRARIES}
    ${UDEV_LIBRARIES})
else()
target_link_libraries(${PROJECT_NAME}
    ${CURSES_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${PROCPS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIBUSB_1_LIBRARIES}
    ${UDEV_LIBRARIES})
endif()

option(NM_DEBUG "Build debug version" OFF)
option(NM_WITH_VNC_CLIENT "Build with usage of external VNC client" ON)
if (NM_DEBUG)
  add_definitions(-DNM_DEBUG -ggdb)
endif()
if (NM_WITH_VNC_CLIENT)
  add_definitions(-DNM_WITH_VNC_CLIENT)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
set(USR_PREFIX "/usr/local" CACHE PATH "Executable and lang files directory prefix")
else()
set(USR_PREFIX "/usr" CACHE PATH "Executable and lang files directory prefix")
endif()
add_definitions(-DNM_USR_PREFIX=${USR_PREFIX})

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    add_definitions(-DNM_OS_LINUX)
    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/sh/convert_kern_ver.sh OUTPUT_VARIABLE LINUX_KERN_VER)
    if (LINUX_KERN_VER GREATER 132641 OR LINUX_KERN_VER EQUAL 132641)
      message(STATUS "Enable sendfile(2) for file copy")
      add_definitions(-DNM_WITH_SENDFILE)
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    add_definitions(-DNM_OS_FREEBSD -I/usr/local/include)
else()
    message(FATAL_ERROR "Build on ${CMAKE_SYSTEM_NAME} is not supported")
endif()

add_subdirectory(lang)
add_dependencies(${PROJECT_NAME} lang)

#configure install
set(QEMU_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.cfg.sample")
set(QEMU_DB_UPGRADE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/upgrade_db.sh")
install(TARGETS ${PROJECT_NAME} DESTINATION ${USR_PREFIX}/bin)
install(FILES ${QEMU_CONFIG_SOURCE} DESTINATION
    "${USR_PREFIX}/share/nemu/templates/config")
install(FILES ${QEMU_DB_UPGRADE_SCRIPT} PERMISSIONS OWNER_EXECUTE OWNER_WRITE
    OWNER_READ  GROUP_EXECUTE GROUP_READ WORLD_EXECUTE
    WORLD_READ DESTINATION "${USR_PREFIX}/share/nemu/scripts")

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG("-std=c99" COMPILER_SUPPORTS_C99)
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS
    "-Wall -Wextra -pedantic -isystem ${LIBUSB_1_INCLUDE_DIRS} ")

if(COMPILER_SUPPORTS_C99)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c99")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_C_COMPILER} has no C99 support. Please use a different C compiler.")
endif()
