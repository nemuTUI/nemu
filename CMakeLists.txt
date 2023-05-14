cmake_minimum_required(VERSION 2.8.12)

project(nemu C)

set(CMAKE_COLOR_MAKEFILE OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

include(GNUInstallDirs)
include(CheckSymbolExists)
include(BuildQEMU)
include(BuildNcurses)
include(FindPkgConfig)
include(FindOpenSSL)
include(FindIntl)

set(CMAKE_C_FLAGS_SANITIZER "-g -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer" CACHE STRING
    "Flags used by the C compiler during sanitizer build."
    FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SANITIZER
    "" CACHE STRING
    "Flags used for linking binaries during sanitizer build."
    FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SANITIZER
    "" CACHE STRING
    "Flags used by the shared libraries linker during sanitizer build."
    FORCE)

mark_as_advanced(
    CMAKE_C_FLAGS_SANITIZER
    CMAKE_EXE_LINKER_FLAGS_SANITIZER
    CMAKE_SHARED_LINKER_FLAGS_SANITIZER)

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel Sanitizer)

set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
    "Build type, options are: Debug Release RelWithDebInfo MinSizeRel Sanitizer."
    FORCE)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(NM_OS_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
  set(NM_OS_FREEBSD TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(NM_OS_DARWIN TRUE)
  add_definitions(-DNM_OS_DARWIN)
else()
  message(FATAL_ERROR "Build on ${CMAKE_SYSTEM_NAME} is not supported")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING "Choose the type of build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
                                               "MinSizeRel" "RelWithDebInfo")
endif()

if(NOT NM_CUSTOM_SYS_INCLUDE)
  set(NM_CUSTOM_SYS_INCLUDE
      ""
      CACHE PATH "Path to additional system include directory" FORCE)
else()
  include_directories(SYSTEM ${NM_CUSTOM_SYS_INCLUDE})
endif()

option(NM_WITH_OVF_SUPPORT "Build with OVF support" ON)
option(NM_WITH_NETWORK_MAP "Build with exporting network map to SVG" OFF)
option(NM_WITH_DBUS "Build with D-Bus support" OFF)
option(NM_WITH_QEMU "Build with embedded QEMU" OFF)
option(NM_WITH_NCURSES "Build with embedded statically linked ncurses" OFF)
option(NM_WITH_REMOTE "Build with remote control" OFF)

include_directories(src)
aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})

if(NM_WITH_NCURSES)
  set(PATCH_COMMAND patch)
  if(NOT CPACK_RPM_PACKAGE_SOURCES)
    build_ncurses()
  endif()
  add_dependencies(${PROJECT_NAME} libncurses libform libpanel)
  set(CURSES_LIBRARIES libncurses libform libpanel)
  add_definitions(-DNM_WITH_NCURSES)
else()
  set(CURSES_NEED_NCURSES TRUE)
  set(CURSES_NEED_WIDE TRUE)
  find_package(Curses REQUIRED)
  if(CURSES_HAVE_CURSES_H)
    add_definitions(-DCURSES_HAVE_CURSES_H="${CURSES_HAVE_CURSES_H}")
  endif()
  if(CURSES_HAVE_NCURSES_CURSES_H)
    add_definitions(-DCURSES_HAVE_NCURSES_CURSES_H="${CURSES_HAVE_NCURSES_CURSES_H}")
  endif()
  if(CURSES_HAVE_NCURSES_H)
    add_definitions(-DCURSES_HAVE_NCURSES_H="${CURSES_HAVE_NCURSES_H}")
  endif()
  if(CURSES_HAVE_NCURSES_NCURSES_H)
    add_definitions(-DCURSES_HAVE_NCURSES_NCURSES_H="${CURSES_HAVE_NCURSES_NCURSES_H}")
  endif()
endif()

find_package(Sqlite3 REQUIRED)
find_package(Threads REQUIRED)
if(NOT NM_OS_DARWIN)
find_package(RT REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(UDev REQUIRED)
endif()
find_package(Intl REQUIRED)
pkg_check_modules(JSONC REQUIRED json-c)

if(NOT NM_OS_DARWIN)
target_link_libraries(
  ${PROJECT_NAME} ${CURSES_LIBRARIES} ${SQLITE3_LIBRARIES} ${JSONC_LINK_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} ${LIBUSB_1_LIBRARIES} ${UDEV_LIBRARIES}
  ${RT_LIBRARY} ${Intl_LIBRARIES})
else()
target_link_libraries(
  ${PROJECT_NAME} ${CURSES_LIBRARIES} ${SQLITE3_LIBRARIES} ${JSONC_LINK_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} ${LIBUSB_1_LIBRARIES} ${Intl_LIBRARIES})
endif()
include_directories(${CURSES_INCLUDE_PATH} ${SQLITE3_INCLUDE_DIR}
                    ${UDEV_INCLUDE_DIR} ${JSONC_INCLUDE_DIRS} ${Intl_INCLUDE_DIRS})
include_directories(SYSTEM ${LIBUSB_1_INCLUDE_DIRS})

add_definitions(-DNM_FULL_DATAROOTDIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}")
add_definitions(-DNM_FULL_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(
  TARGET ${PROJECT_NAME}
  APPEND_STRING
  PROPERTY COMPILE_FLAGS "-Wall -Wextra -Wformat-security -pedantic")
set_property(
  TARGET ${PROJECT_NAME}
  APPEND_STRING
  PROPERTY LINK_FLAGS_RELEASE "-s")

set(NM_WITH_SENDFILE FALSE)

set(NM_DEFAULT_VMDIR "nemu_vm" CACHE STRING
  "Default VM directory with subdirectories in users home dir")
set(NM_DEFAULT_VNC "/usr/bin/vncviewer" CACHE STRING
  "Default VNC client")
set(NM_DEFAULT_DBFILE ".nemu.db" CACHE STRING
  "Default database file name with subdirectories in users home dir")
set(NM_DEFAULT_SPICE "/usr/bin/remote-viewer" CACHE STRING
  "Default SPICE client")
set(NM_DEFAULT_QEMUDIR "/usr/bin" CACHE STRING
  "Default qemu binaries directory path")

if(NM_OS_LINUX)
  add_definitions(-DNM_OS_LINUX)

  try_compile(
    NET_IF
    "${CMAKE_CURRENT_BINARY_DIR}/CMake_Tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Tests/net_if.c"
    COMPILE_DEFINITIONS "-Wall -Wextra -pedantic -Werror")

  if(NOT NET_IF)
    add_definitions(-DNM_NET_IF_FIX)
  endif()

  try_compile(
    SENDFILE
    "${CMAKE_CURRENT_BINARY_DIR}/CMake_Tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Tests/sendfile.c"
    COMPILE_DEFINITIONS "-Wall -Wextra -pedantic -Werror")

  if(SENDFILE)
    set(NM_WITH_SENDFILE TRUE)
    add_definitions(-DNM_WITH_SENDFILE)
  endif()

  if(NM_CUSTOM_SYS_INCLUDE)
    check_symbol_exists(
      RTM_NEWLINKPROP "${NM_CUSTOM_SYS_INCLUDE}/linux/rtnetlink.h"
      NM_WITH_NEWLINKPROP)
  else()
    check_symbol_exists(RTM_NEWLINKPROP "linux/rtnetlink.h" NM_WITH_NEWLINKPROP)
  endif()
  if(NM_WITH_NEWLINKPROP)
    set(NM_WITH_NEWLINKPROP TRUE)
    add_definitions(-DNM_WITH_NEWLINKPROP)
  else()
    set(NM_WITH_NEWLINKPROP FALSE)
  endif()
elseif(NM_OS_FREEBSD)
  add_definitions(-DNM_OS_FREEBSD -D_BSD_SOURCE)
  set(NM_WITH_NETWORK_MAP FALSE)
  find_package(Intl REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${Intl_LIBRARIES})
endif()

if(NM_WITH_OVF_SUPPORT)
  find_package(LibXml2 REQUIRED)
  find_package(LibArchive REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${LIBXML2_LIBRARIES}
                        ${LibArchive_LIBRARY})
  include_directories(${LIBXML2_INCLUDE_DIR} ${LibArchive_INCLUDE_DIR})
  add_definitions(-DNM_WITH_OVF_SUPPORT)
endif()

if(NM_WITH_NETWORK_MAP)
  find_package(Graphviz REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${GRAPHVIZ_LIBRARIES})
  include_directories(${GRAPHVIZ_INCLUDE_DIRS})
  add_definitions(-DNM_WITH_NETWORK_MAP)
endif()

if(NM_WITH_DBUS)
  find_package(DBus REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${DBUS_LIBRARIES})
  include_directories(${DBUS_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})
  add_definitions(-DNM_WITH_DBUS)
endif()

if(NM_WITH_REMOTE)
  find_package(OpenSSL 1.1 REQUIRED)
  target_link_libraries(${PROJECT_NAME} ssl)
  target_link_libraries(${PROJECT_NAME} crypto)
  include_directories(${OPENSSL_INCLUDE_DIR})
  add_definitions(-DNM_WITH_REMOTE)
endif()

if(NM_WITH_QEMU)
  set(PATCH_COMMAND patch)
  if(NOT NM_QEMU_TARGET_LIST)
    set(NM_QEMU_TARGET_LIST
        "x86_64-softmmu,i386-softmmu"
        CACHE STRING "List of embedded QEMU targets" FORCE)
  endif()
  if(NOT CPACK_RPM_PACKAGE_SOURCES)
    build_qemu()
  endif()
  set(NM_DEFAULT_QEMUDIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/nemu/qemu/bin" CACHE STRING
    "Default qemu binaries directory path" FORCE)
  add_definitions(-DNM_WITH_QEMU)
endif()

if(NOT GIT_TAG_VERSION)
  execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/sh/git_get_version.sh
            ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE GIT_TAG_VERSION)
endif()
if(NOT GIT_TAG_VERSION STREQUAL "")
  message(STATUS "Build from git, version:${GIT_TAG_VERSION}")
  add_definitions(-DNM_VERSION="${GIT_TAG_VERSION}")
endif()
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

add_definitions(-DNM_DEFAULT_VMDIR="${NM_DEFAULT_VMDIR}")
add_definitions(-DNM_DEFAULT_VNC="${NM_DEFAULT_VNC}")
add_definitions(-DNM_DEFAULT_DBFILE="${NM_DEFAULT_DBFILE}")
add_definitions(-DNM_DEFAULT_SPICE="${NM_DEFAULT_SPICE}")
add_definitions(-DNM_DEFAULT_QEMUDIR="${NM_DEFAULT_QEMUDIR}")

add_subdirectory(lang)
add_dependencies(${PROJECT_NAME} lang)

add_subdirectory(man)
add_dependencies(${PROJECT_NAME} man)

add_subdirectory(test)

# configure install
set(NEMU_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.cfg.sample")
set(NEMU_DB_UPGRADE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/upgrade_db.sh")
set(NEMU_NON_ROOT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/setup_nemu_nonroot.sh")
set(NEMU_MACVTAP_UDEV_RULE
    "${CMAKE_CURRENT_SOURCE_DIR}/sh/42-net-macvtap-perm.rules")
set(NEMU_NTTY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/ntty")
set(NEMU_BASH_AUTOCOMPLETE "${CMAKE_CURRENT_SOURCE_DIR}/sh/nemu.bash")
set(NEMU_ZSH_AUTOCOMPLETE "${CMAKE_CURRENT_SOURCE_DIR}/sh/nemu.zsh")

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  FILES ${NEMU_CONFIG_SOURCE}
  PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/nemu/templates/config)
install(
  FILES ${NEMU_MACVTAP_UDEV_RULE}
  PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
  DESTINATION share/nemu/scripts)
install(
  FILES ${NEMU_DB_UPGRADE_SCRIPT} ${NEMU_NON_ROOT_SCRIPT}
  PERMISSIONS
    OWNER_EXECUTE
    OWNER_WRITE
    OWNER_READ
    GROUP_EXECUTE
    GROUP_READ
    WORLD_EXECUTE
    WORLD_READ
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/nemu/scripts)
install(
  FILES ${NEMU_NTTY_SCRIPT}
  PERMISSIONS
    OWNER_EXECUTE
    OWNER_WRITE
    OWNER_READ
    GROUP_EXECUTE
    GROUP_READ
    WORLD_EXECUTE
    WORLD_READ
  DESTINATION ${CMAKE_INSTALL_BINDIR})
if(NM_WITH_QEMU)
  install(
    DIRECTORY
      ${CMAKE_CURRENT_BINARY_DIR}/qemu/${CMAKE_INSTALL_FULL_DATAROOTDIR}/nemu/qemu/
      USE_SOURCE_PERMISSIONS
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/nemu/qemu)
endif()
install(
  FILES ${NEMU_BASH_AUTOCOMPLETE}
  PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
  DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/bash-completion/completions
  RENAME "nemu")
install(
  FILES ${NEMU_ZSH_AUTOCOMPLETE}
  PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
  DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/zsh/site-functions
  RENAME "_nemu")

# uninstall target
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)

add_custom_target(
  uninstall COMMAND ${CMAKE_COMMAND} -P
                    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

add_subdirectory(pkg/linux/rpm)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Default VM directory name: ${NM_DEFAULT_VMDIR}")
message(STATUS "Default VNC client ${NM_DEFAULT_VNC}")
message(STATUS "Default database name: ${NM_DEFAULT_DBFILE}")
message(STATUS "Default SPICE client ${NM_DEFAULT_SPICE}")
message(STATUS "Default qemu binaries directory path: ${NM_DEFAULT_QEMUDIR}")
if(NM_CUSTOM_SYS_INCLUDE)
  message(
    STATUS "Additional system include directory: ${NM_CUSTOM_SYS_INCLUDE}")
endif()
message(STATUS "Using sendfile: ${NM_WITH_SENDFILE}")
message(
  STATUS
    "Using alternative names for network interfaces: ${NM_WITH_NEWLINKPROP}")
message(STATUS "OVF support: ${NM_WITH_OVF_SUPPORT}")
message(STATUS "Network map export: ${NM_WITH_NETWORK_MAP}")
message(STATUS "D-Bus support: ${NM_WITH_DBUS}")
message(STATUS "Remote control support: ${NM_WITH_REMOTE}")
message(STATUS "Embedded QEMU: ${NM_WITH_QEMU}")
if(NM_WITH_QEMU)
  message(STATUS "Embedded QEMU targets: ${NM_QEMU_TARGET_LIST}")
endif()
message(STATUS "Embedded ncurses: ${NM_WITH_NCURSES}")
