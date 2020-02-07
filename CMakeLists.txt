include(CheckSymbolExists)
cmake_minimum_required(VERSION 2.8)
project(nemu C)
set(CMAKE_COLOR_MAKEFILE OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE
      PATH "Executable and lang files directory prefix" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE
      PATH "Executable and lang files directory prefix" FORCE)
  endif()
endif()
add_definitions(-DNM_USR_PREFIX=${CMAKE_INSTALL_PREFIX})
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (NOT NM_CUSTOM_SYS_INCLUDE)
  set(NM_CUSTOM_SYS_INCLUDE "" CACHE
    PATH "Path to custom include directory" FORCE)
endif()
if (NM_CUSTOM_SYS_INCLUDE)
  include_directories(SYSTEM ${NM_CUSTOM_SYS_INCLUDE})
endif()

option(NM_DEBUG "Build debug version" OFF)
option(NM_WITH_OVF_SUPPORT "Build with OVF support" ON)
option(NM_SAVEVM_SNAPSHOTS "Build with QEMU savevm snapshots" OFF)
option(NM_WITH_VNC_CLIENT "Build with usage of external VNC client" ON)
option(NM_WITH_SPICE "Build with Spice protocol support" ON)
option(NM_WITH_NETWORK_MAP "Build with exporting network map to SVG" OFF)

if (NM_DEBUG)
  add_definitions(-DNM_DEBUG -ggdb)
endif()
if (NM_WITH_VNC_CLIENT)
  add_definitions(-DNM_WITH_VNC_CLIENT)
endif()
if (NM_WITH_SPICE)
  add_definitions(-DNM_WITH_SPICE)
endif()
if (NM_SAVEVM_SNAPSHOTS)
  add_definitions(-DNM_SAVEVM_SNAPSHOTS)
endif()
if (NM_WITH_OVF_SUPPORT)
  add_definitions(-DNM_WITH_OVF_SUPPORT)
endif()
if (NM_WITH_NETWORK_MAP)
  add_definitions(-DNM_WITH_NETWORK_MAP)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
  if (NM_CUSTOM_SYS_INCLUDE)
    set(RTNETLINK_PATH "${NM_CUSTOM_SYS_INCLUDE}/linux/rtnetlink.h")
  else()
    set(RTNETLINK_PATH "linux/rtnetlink.h")
  endif()

  check_symbol_exists(RTM_NEWLINKPROP ${RTNETLINK_PATH} HAVE_NEWLINKPROP)

  unset(RTNETLINK_PATH)

  if (HAVE_NEWLINKPROP)
    add_definitions(-DNM_WITH_NEWLINKPROP)
  endif()
endif()

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(libusb-1.0 REQUIRED)
find_package(UDev REQUIRED)
if (NM_WITH_OVF_SUPPORT)
  find_package(LibXml2 REQUIRED)
  find_package(LibArchive REQUIRED)
endif()
if (NM_WITH_NETWORK_MAP)
  find_package(Graphviz REQUIRED)
endif()
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
    ${LIBXML2_LIBRARIES}
    ${UDEV_LIBRARIES})
endif()

if (NM_WITH_OVF_SUPPORT)
  target_link_libraries(${PROJECT_NAME}
      ${LIBXML2_LIBRARIES}
      ${LibArchive_LIBRARY})
  include_directories(${LIBXML2_INCLUDE_DIR})
endif()

if (NM_WITH_NETWORK_MAP)
  target_link_libraries(${PROJECT_NAME}
      ${GRAPHVIZ_LIBRARIES})
  include_directories(${GRAPHVIZ_INCLUDE_DIRS})
endif()

execute_process(COMMAND ${CMAKE_SOURCE_DIR}/sh/git_get_version.sh ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_TAG_VERSION)
if (NOT GIT_TAG_VERSION STREQUAL "")
    message(STATUS "Build from git, version:${GIT_TAG_VERSION}")
    add_definitions(-DNM_VERSION="${GIT_TAG_VERSION}")
endif()

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
set(NEMU_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.cfg.sample")
set(NEMU_DB_UPGRADE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/upgrade_db.sh")
set(NEMU_NON_ROOT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/setup_nemu_nonroot.sh")
set(NEMU_MACVTAP_UDEV_RULE "${CMAKE_CURRENT_SOURCE_DIR}/sh/42-net-macvtap-perm.rules")
set(NEMU_NTTY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/sh/ntty")
set(NEMU_BASH_AUTOCOMPLETE "${CMAKE_CURRENT_SOURCE_DIR}/sh/nemu.bash")
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES ${NEMU_CONFIG_SOURCE} DESTINATION share/nemu/templates/config)
install(FILES ${NEMU_MACVTAP_UDEV_RULE} PERMISSIONS
    OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
    DESTINATION share/nemu/scripts)
install(FILES ${NEMU_DB_UPGRADE_SCRIPT} ${NEMU_NON_ROOT_SCRIPT} PERMISSIONS
    OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE
    WORLD_READ DESTINATION share/nemu/scripts)
install(FILES ${NEMU_NTTY_SCRIPT} PERMISSIONS
    OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE
    WORLD_READ DESTINATION bin)
if(EXISTS ${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions)
  install(FILES ${NEMU_BASH_AUTOCOMPLETE} PERMISSIONS
      OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
      DESTINATION share/bash-completion/completions
      RENAME "nemu")
endif()

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
    "-Wall -Wextra -Wformat-security -pedantic -isystem ${LIBUSB_1_INCLUDE_DIRS} ")

if(COMPILER_SUPPORTS_C99)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c99")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_C_COMPILER} has no C99 support. Please use a different C compiler.")
endif()

add_subdirectory(pkg/linux/rpm)
