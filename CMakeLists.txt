cmake_minimum_required(VERSION 2.8)

project(nemu C)

set(CMAKE_COLOR_MAKEFILE OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(GNUInstallDirs)
include(GetGLibcVersion)
include(CheckSymbolExists)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(NM_OS_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
  set(NM_OS_FREEBSD TRUE)
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
option(NM_SAVEVM_SNAPSHOTS "Build with QEMU savevm snapshots" OFF)
option(NM_WITH_VNC_CLIENT "Build with usage of external VNC client" ON)
option(NM_WITH_SPICE "Build with Spice protocol support" ON)
option(NM_WITH_NETWORK_MAP "Build with exporting network map to SVG" OFF)
option(NM_WITH_DBUS "Build with D-Bus support" OFF)

include_directories(src)
aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(UDev REQUIRED)

target_link_libraries(
  ${PROJECT_NAME} ${CURSES_LIBRARIES} ${SQLITE3_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} ${LIBUSB_1_LIBRARIES} ${UDEV_LIBRARIES})
include_directories(${CURSES_INCLUDE_PATH} ${SQLITE3_INCLUDE_DIR}
                    ${UDEV_INCLUDE_DIR})
include_directories(SYSTEM ${LIBUSB_1_INCLUDE_DIRS})

add_definitions(-DNM_FULL_DATAROOTDIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}")
add_definitions(-DNM_FULL_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(
  TARGET ${PROJECT_NAME}
  APPEND_STRING
  PROPERTY COMPILE_FLAGS "-Wall -Wextra -Wformat-security -pedantic -std=c99 ")

set(NM_WITH_SENDFILE FALSE)

if(NM_OS_LINUX)
  add_definitions(-DNM_OS_LINUX)

  get_glibc_version()
  if(GLIBC_VERSION VERSION_GREATER 2.18)
    add_definitions(-D_DEFAULT_SOURCE)
  else()
    add_definitions(-D_XOPEN_SOURCE=700 -D_BSD_SOURCE)
  endif()

  if(GLIBC_VERSION VERSION_GREATER 2.0)
    set(NM_WITH_SENDFILE TRUE)
    add_definitions(-DNM_WITH_SENDFILE)
  endif()

  if(NM_CUSTOM_SYS_INCLUDE)
    check_symbol_exists(
      RTM_NEWLINKPROP
      "${NM_CUSTOM_SYS_INCLUDE}/linux/rtnetlink.h"
      NM_WITH_NEWLINKPROP)
  else()
    check_symbol_exists(
      RTM_NEWLINKPROP
      "linux/rtnetlink.h"
      NM_WITH_NEWLINKPROP)
  endif()
  if(NM_WITH_NEWLINKPROP)
    set(NM_WITH_NEWLINKPROP TRUE)
    add_definitions(-DNM_WITH_NEWLINKPROP)
  else()
    set(NM_WITH_NEWLINKPROP FALSE)
  endif()
elseif(NM_OS_FREEBSD)
  add_definitions(-DNM_OS_FREEBSD -D_BSD_SOURCE)
  find_package(Intl REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${Intl_LIBRARIES})
endif()

if(NM_WITH_VNC_CLIENT)
  add_definitions(-DNM_WITH_VNC_CLIENT)
endif()

if(NM_WITH_SPICE)
  add_definitions(-DNM_WITH_SPICE)
endif()

if(NM_SAVEVM_SNAPSHOTS)
  add_definitions(-DNM_SAVEVM_SNAPSHOTS)
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

if(NOT GIT_TAG_VERSION)
  execute_process(COMMAND ${CMAKE_SOURCE_DIR}/sh/git_get_version.sh
                          ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_TAG_VERSION)
endif()
if(NOT GIT_TAG_VERSION STREQUAL "")
  message(STATUS "Build from git, version:${GIT_TAG_VERSION}")
  add_definitions(-DNM_VERSION="${GIT_TAG_VERSION}")
endif()

add_subdirectory(lang)
add_dependencies(${PROJECT_NAME} lang)

# configure install
set(NEMU_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.cfg.sample")
set(NEMU_DB_UPGRADE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/upgrade_db.sh")
set(NEMU_NON_ROOT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/setup_nemu_nonroot.sh")
set(NEMU_MACVTAP_UDEV_RULE
    "${CMAKE_CURRENT_SOURCE_DIR}/sh/42-net-macvtap-perm.rules")
set(NEMU_NTTY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/ntty")
set(NEMU_BASH_AUTOCOMPLETE "${CMAKE_CURRENT_SOURCE_DIR}/sh/nemu.bash")

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
  FILES ${NEMU_BASH_AUTOCOMPLETE}
  PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
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
if(EXISTS ${CMAKE_INSTALL_FULL_DATAROOTDIR}/bash-completion/completions)
  install(
    FILES ${NEMU_BASH_AUTOCOMPLETE}
    PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/bash-completion/completions
    RENAME "nemu")
endif()

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
if(NM_CUSTOM_SYS_INCLUDE)
  message(
    STATUS "Additional system include directory: ${NM_CUSTOM_SYS_INCLUDE}")
endif()
message(STATUS "Using sendfile: ${NM_WITH_SENDFILE}")
message(
  STATUS
    "Using alternative names for network interfaces: ${NM_WITH_NEWLINKPROP}")
message(STATUS "OVF support: ${NM_WITH_OVF_SUPPORT}")
message(STATUS "QEMU snapshots: ${NM_SAVEVM_SNAPSHOTS}")
message(STATUS "VNC/Spice support: ${NM_WITH_VNC_CLIENT}")
message(STATUS "Spice support: ${NM_WITH_SPICE}")
message(STATUS "Network map export: ${NM_WITH_NETWORK_MAP}")
message(STATUS "D-Bus support: ${NM_WITH_DBUS}")
