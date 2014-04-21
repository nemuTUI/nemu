# - Try to find procps or procps-ng library
#
# -----------------------------------------------------------------------------
#  Once done these variables will be defined
# -----------------------------------------------------------------------------
#
#  PROCPS_FOUND          - System has suitable libprocps
#  PROCPS_INCLUDE_DIRS   - The libprocps include directories
#  PROCPS_LIBRARIES      - The libraries needed to use libprocps
#  PROCPS_VERSION_STRING - libprocps version
#
# Following two variables are udpated only if PROCPS_FOUND is true
#
#  PROCPS_MISSING_SYMBOLS            - list of missing symbols
#                                      (see PROCPS_CHECK_SYMBOLS)
#  PROCPS_MISSING_PROC_T_MEMBERS     - list of missing proc_t members
#                                      (see PROCPS_CHECK_PROC_T_MEMBERS)
#  PROCPS_HAS_<symbol>
#  PROCPS_PROC_T_HAS_MEMBER_<member>
#
# -----------------------------------------------------------------------------
#  Variables you can set before calling find_package(Procps ...)
# -----------------------------------------------------------------------------
#
#  PROCPS_STATIC                 - set ON for static linkage (CACHED OPTION)
#  PROCPS_HEADERS                - required headers, defaults to list:
#                                    "proc/procps.h"
#                                    "proc/sysinfo.h"
#                                    "proc/pwcache.h"
#                                    "proc/readproc.h"
#
#  PROCPS_REQUIRE_NG             - if set, require procps-ng fork
#  PROCPS_CHECK_SYMBOLS          - list of symbols to check
#  PROCPS_CHECK_PROC_T_MEMBERS   - list of proc_t members to check
#  PROCPS_REQUIRE_SYMBOLS        - list of required symbols from
#                                  PROCPS_CHECK_SYMBOLS, any of these missing
#                                  will cause failure. 
#  PROCPS_REQUIRE_PROC_T_MEMBERS - list of required proc_t members from
#                                  PROCPS_CHECK_PROC_T_MEMBERS, any of these
#                                  missing will cause failure.
#  Procps_VERBOSE                - be more verbose
#  Procps_DEBUG                  - output debug info
#
# Location of library and include dir.
#   1) If the variable is defined to a value that is not a false constant it
#      will be used as the location.
#   2) If the variable is defined to a value that is false constant (i.e. 0,
#      OFF, NO, FALSE, N, IGNORE, empty string, or ends in the suffix
#      '-NOTFOUND') the location will be (re)detected and variable updated to
#      that.
#
#  PROCPS_STATIC_LIBRARY     - location of library for static linkage
#                              (CACHED FILEPATH)
#  PROCPS_STATIC_INCLUDE_DIR - include dir for static linkage (CACHED PATH)
#  PROCPS_SHARED_LIBRARY     - location of shared library (CACHED FILEPATH)  
#  PROCPS_SHARED_INCLUDE_DIR - include dir for dynamic linkage (CACHED PATH)
#
#  PROCPS_LIBRARY            - overrides PROCPS_SHARED_LIBRARY or
#                              PROCPS_STATIC_LIBRARY if PROCPS_STATIC is ON
#  PROCPS_INCLUDE_DIR        - overrides PROCPS_SHARED_INCLUDE_DIR or
#                              PROCPS_STATIC_INCLUDE_DIR if PROCPS_STATIC is ON
#
#  PROCPS_LIBRARY and PROCPS_INCLUDE_DIR will be removed from the cache after
#  their value is copied to PROCPS_(STATIC|SHARED)_* variables.
#
#  This module detects changes in the input variables and in the library and
#  header files and internally tracks separate results for both static and
#  dynamic linking. It is done this way because the unstardardized and possibly
#  changing procps API may require the user to try several procps versions
#  or switch between dynamic and static linking. I wanted to avoid the need to
#  clean the cache each time.
#  
#  Because of this you don't need to clean the CMake cache to reconfigure
#  procps. If you need just change PROCPS_STATIC, PROCPS_LIBRARY and
#  PROCPS_INCLUDE_DIR variable. Simple running of "cmake ." should be
#  sufficient to reconfigure the project.
#  
# -----------------------------------------------------------------------------
#  Example usage
# -----------------------------------------------------------------------------
#
# set(PROCPS_CHECK_SYMBOLS             # list of required libprocps symbols
#       "readtask" "readproc"
#       "openproc" "closeproc" "freeproc"
#       "uptime" "loadavg" "smp_num_cpus"
#       "user_from_uid" "group_from_gid"
#       "get_pid_digits"
#
#       "meminfo"
#       "kb_active" "kb_main_shared" "kb_main_buffers" "kb_main_cached"
#       "kb_main_free" "kb_main_total" "kb_swap_free" "kb_swap_total"
#       "kb_high_free" "kb_high_total" "kb_low_free" "kb_low_total" "kb_active"
#       "kb_inact_laundry" "kb_inact_dirty" "kb_inact_clean" "kb_inact_target"
#       "kb_swap_cached" "kb_swap_used" "kb_main_used" "kb_writeback" "kb_slab"
#       "kb_committed_as" "kb_dirty" "kb_inactive" "kb_mapped" "kb_pagetables"
#
#       "vminfo"
#       "vm_nr_dirty" "vm_nr_writeback" "vm_nr_pagecache" "vm_nr_mapped"
#       "vm_nr_reverse_maps" "vm_nr_page_table_pages" "vm_nr_slab" "vm_pgpgin"
#       "vm_pgpgout" "vm_pswpin" "vm_pswpout" "vm_pgalloc" "vm_pgfree"
#       "vm_pgactivate" "vm_pgdeactivate" "vm_pgfault" "vm_pgmajfault"
#       "vm_pgscan" "vm_pgrefill" "vm_pgsteal" "vm_kswapd_steal" "vm_pageoutrun"
#       "vm_allocstall"
#   )
# set(PROCPS_REQUIRE_SYMBOLS ${PROCPS_CHECK_SYMBOLS}) # require all
#
# set(PROCPS_CHECK_PROC_T_MEMBERS    # list of required proc_t structure members
#       "tid" "ppid" "state" "utime" "stime" "cutime" "cstime" "start_time"
#       "signal" "blocked" "sigignore" "sigcatch" "_sigpnd" "start_code"
#       "end_code" "start_stack" "kstk_esp" "kstk_eip" "wchan" "priority"
#       "nice" "rss" "alarm" "size" "resident" "share" "trs" "lrs" "drs" "dt"
#       "vm_size" "vm_lock" "vm_rss" "vm_data" "vm_stack" "vm_swap" "vm_exe"
#       "vm_lib" "rtprio" "sched" "vsize" "rss_rlim" "flags" "min_flt" "maj_flt"
#       "cmin_flt" "cmaj_flt" "euser" "ruser" "suser" "fuser" "rgroup" "egroup"
#       "sgroup" "fgroup" "cmd" "nlwp" "tgid" "tty" "euid" "egid" "ruid" "rgid"
#       "suid" "sgid" "fuid" "fgid" "tpgid" "exit_signal" "processor"
#   )
# set(PROCPS_REQUIRE_PROC_T_MEMBERS ${PROCPS_CHECK_PROC_T_MEMBERS}) #require all
# set(Procps_VERBOSE ON)
#
# set(PROCPS_REQUIRE_NG ON) # set OFF if you want to try older procps
# find_package(Procps "3.3.0" REQUIRED)
#
# include_directories(${PROCPS_INCLUDE_DIRS})
# set(LIBS ${LIBS} ${PROCPS_LIBRARIES})
#
#
#=============================================================================
# Copyright 2014 Petr Gajdůšek <gajdusek.petr@centrum.cz>
#
# Distributed under the OSI-approved BSD License (the "License")
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

INCLUDE(FindPkgConfig)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckStructHasMember)
INCLUDE(FindPackageHandleStandardArgs)

# -----------------------------------------------------------------------------
#  Options and help descriptions
# -----------------------------------------------------------------------------

set(desc_PROCPS_STATIC      # list will be displayed joined with semi-colon
  "Do static linkage to libprocps"
  " this is usually required because procps API is not standardized"
  " turn this OFF only if you have patched libprocps to export all needed symbols.")

option(PROCPS_STATIC "${desc_PROCPS_STATIC}" TRUE)

set(help_PROCPS_STATIC_LOCATION
  "You can override location of libprocps library to which you want statically"
  " link and location of the include directory by setting PROCPS_STATIC_LIBRARY"
  " and PROCPS_STATIC_INCLUDE_DIR cmake variables.\n"
  "   PROCPS_STATIC_LIBRARY       specifies full path to the library\n"
  "                               (i.e. path to the libprocps.a file)\n"
  "   PROCPS_STATIC_INCLUDE_DIR   specifies directory which contains\n"
  "                               the proc/procps.h header.\n"
  "You may achieve this e.g. by running\n"
  " ## cmake -D PROCPS_STATIC_LIBRARY:FILEPATH=/path/to/libprocps.a"
  " -D PROCPS_STATIC_INCLUDE_DIR:PATH=/path/to/include/dir .\n")

set(help_PROCPS_SHARED
  "If you insist on dynamic linkage to shared libprocps, update or patch"
  " libprocps to export all symbols that application needs. If you"
  " manage your solution to be persistent, future proof (in sense of"
  " API changes) and officially accepted by your GNU/Linux"
  " distribution, please contact authors."
  "\n"
  "Otherwise consider static linkage to libprocps by setting cmake"
  " variable PROCPS_STATIC, e.g. by running\n"
  " ## cmake -D PROCPS_STATIC:BOOL=ON .\n" )

set(help_PROCPS_SHARED_LOCATION
  "You can override location of shared libprocps library and the include"
  " directory by setting PROCPS_SAHRED_LIBRARY and PROCPS_SHARED_INCLUDE_DIR"
  " cmake variables.\n"
  "   PROCPS_SHARED_LIBRARY       specifies full path to the library\n"
  "                               (i.e. path to the libprocps.so file)\n"
  "   PROCPS_SHARED_INCLUDE_DIR   specifies directory which contains\n"
  "                               the proc/procps.h header.\n"
  "You may achieve this e.g. by running\n"
  " ## cmake -D PROCPS_SHARED_LIBRARY:FILEPATH=/path/to/libprocps.so"
  " -D PROCPS_SHARED_INCLUDE_DIR:PATH=/path/to/include/dir .\n" )



# -----------------------------------------------------------------------------
#  Miscellaneous macros
# -----------------------------------------------------------------------------

# debug
macro(print_procps_variables)
 foreach(_varname SHARED_LIBRARY STATIC_LIBRARY
                  SHARED_INCLUDE_DIR STATIC_INCLUDE_DIR
                  SHARED_VERSION_STRING STATIC_VERSION_STRING)
   message(STATUS
           "\${__procps_saved_${_varname}} = ${__procps_saved_${_varname}} "
           "    \${PROCPS_${_varname}} = ${PROCPS_${_varname}}" )
 endforeach()
endmacro()

# debug
macro(print_all_variables)
  get_cmake_property(_variableNames VARIABLES)
  foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
  endforeach()
endmacro()

#
# file_stat: get status of file that you may be later compare to detect changes
# file_stat(<VAR>, filepath)
#
macro(file_stat var filepath )
  if(EXISTS "${filepath}" )
    execute_process(
        COMMAND "stat" "-L" "--printf=%i %Y %u:%g %a %s %n" "${filepath}"
        OUTPUT_VARIABLE ${var}
        ERROR_QUIET )
  else()
    set(${var} "${filepath}-NOTFOUND") 
  endif()
  msg_debug("file_stat(\"${filepath}\") ${var} = \"${${var}}\"")
endmacro()

#
# save_files_stat: Internally save stat of library and headers files
# save_files_stat( "STATIC" | "SHARED" )
# You can call detect_changed_files()
#
function(save_files_stat linkage)
  msg_debug("Saving file stats.")
  unset(INCLUDE_DIR_FILES)
  foreach(h ${PROCPS_HEADERS})
    set(INCLUDE_DIR_FILES
        ${INCLUDE_DIR_FILES} "${PROCPS_${linkage}_INCLUDE_DIR}/${h}")
  endforeach()

  set(LIBRARY_FILES "${PROCPS_${linkage}_LIBRARY}")

  foreach(_var "LIBRARY" "INCLUDE_DIR")
    unset(stats)
    if(PROCPS_${linkage}_${_var} AND EXISTS "${PROCPS_${linkage}_${_var}}")
      foreach(f ${${_var}_FILES})
        unset(stat)
        file_stat(stat "${f}")
        set(stats ${stats} "${stat}")
      endforeach()
      set(__procps_saved_filestat_${linkage}_${_var} "${stats}" CACHE INTERNAL "")
    else()
      unset(__procps_saved_filestat_${linkage}_${_var} CACHE)
    endif()
    msg_debug("save __procps_saved_filestat_${linkage}_${_var}: "
              "\"${__procps_saved_filestat_${linkage}_${_var}}\"" ) 
  endforeach()
endfunction()

#
# detect_changed_files: Detect if library or headers files has been changed.
# Usage: detect_changed_files ( "STATIC" | "SHARED" )
# 
# Detect if library or headers files has been changed since last
# save_files_stat(). Sets internal variables 
# PROCPS_CHANGED_{LIBRARY,INCLUDE_DIR,VERSION_STRING} to ON/OFF.
#

function(detect_changed_files linkage)
  msg_debug("Detecting if file stats changed.")
  unset(INCLUDE_DIR_FILES)
  foreach(h ${PROCPS_HEADERS})
    set(INCLUDE_DIR_FILES
        ${INCLUDE_DIR_FILES} "${PROCPS_${linkage}_INCLUDE_DIR}/${h}")
  endforeach()

  set(LIBRARY_FILES "${PROCPS_${linkage}_LIBRARY}")

  foreach(_var "LIBRARY" "INCLUDE_DIR")
    set(changed TRUE)
    if(DEFINED __procps_saved_filestat_${linkage}_${_var}
            AND PROCPS_${linkage}_${_var} 
            AND EXISTS "${PROCPS_${linkage}_${_var}}" )
      msg_debug("Detecting changes for PROCPS_${linkage}_${_var}.")
      unset(stats)
      foreach(f ${${_var}_FILES})
        unset(stat)
        file_stat(stat "${f}")
        set(stats ${stats} "${stat}")
      endforeach()
      if(stats AND
            "${stats}" STREQUAL "${__procps_saved_filestat_${linkage}_${_var}}")
        set(changed FALSE)
      endif()
    endif()
    set(PROCPS_CHANGED_${_var} "${changed}")
    set(PROCPS_CHANGED_${_var} "${changed}" PARENT_SCOPE)
    msg_debug("PROCPS_CHANGED_${_var} = ${PROCPS_CHANGED_${_var}}")
  endforeach()
endfunction()

#
# status message
#
macro(msg)
  if(NOT PROCPS_CONFIGURED OR Procps_DEBUG)
    message(STATUS ${ARGV})
  endif()
endmacro()

#
# status verbose message
#
macro(msg_verbose)
  if(Procps_VERBOSE AND NOT Procps_FIND_QUIETLY AND NOT PROCPS_CONFIGURED
     OR Procps_DEBUG
    )
    message(STATUS "  " ${ARGV})
  endif()
endmacro()

#
# status debug message
#
macro(msg_debug)
  if(Procps_DEBUG)
    message(STATUS "  D: " ${ARGV})
  endif()
endmacro()

unset(errors)

#
# error: Add error.
# error("error message to display" ...)
#
# Add error message to `errors` list and if Procps_VERBOSE is set, print
# the message to the user.
#
macro(error)
  if(Procps_FIND_REQUIRED)
    msg_verbose("E: ${ARGV}")
  else()
    msg_verbose("W: ${ARGV}")
  endif()
  
  # join list
  unset(error_msg)
  foreach(msg ${ARGV})
    set(error_msg "${error_msg}${msg}")
  endforeach()

  if (DEFINED errors) 
    set(errors ${errors} "${error_msg}")
  else()
    set(errors "${error_msg}")
  endif()
endmacro()

#
# fail: Fail module.
# fail("error message to display" ...)
#
# Fail the module and print all errors from the `errors` list as reasons
# of failure. Optional arguments are appended to the end of the `errors` list
# before printing.
#
macro(fail)
  if(${ARGC} GREATER 0)
    error(${ARGV})
  endif()
  if(PROCPS_STATIC)
    set(_msg "Library ${log_libname} for static linking not found.")
    set(_help ${help_PROCPS_STATIC_LOCATION})
  else()
    set(_msg "Library ${log_libname} for dynamic linking not found.")
    set(_help ${help_PROCPS_SHARED_LOCATION} "\n" ${help_PROCPS_SHARED} )
  endif()

  if(Procps_FIND_REQUIRED)
    unset(reasons)
    foreach(err ${errors})
      if(reasons)
        set(reasons "${reasons}\n REASON: " ${err})
      else()
        set(reasons " REASON: " ${err})
      endif()
    endforeach()
    message(SEND_ERROR ${_msg} "\n" ${reasons} "\n" ${_help} )
  elseif(NOT Procps_FIND_QUIETLY)
    message(STATUS "  W: " ${_msg} )
    foreach(err ${errors})
      message(STATUS "  REASON: " ${err} ) 
    endforeach()
  else()
    message(STATUS "  not found procps")
  endif()
  unset(__procps_saved_OK CACHE)
  return()
endmacro()

#
# check_failed: Fail the module if some errors were added with error() macro.
#
macro(check_failed)
  if (DEFINED errors)
    fail()
  endif()
endmacro()

#
# finish
#
macro(finish)
  find_package_handle_standard_args(Procps
                                    REQUIRED_VARS PROCPS_LIBRARIES
                                                  PROCPS_INCLUDE_DIRS
                                                  PROCPS_VERSION_STRING
                                    VERSION_VAR PROCPS_VERSION_STRING )
  return()
endmacro()

# -----------------------------------------------------------------------------
#  Begin setup
# -----------------------------------------------------------------------------

msg_debug("FindProcps starts.")

## setup linkage type (static or dynamic)
if (PROCPS_STATIC)
  set(desc_PROCPS_STATIC_LIBRARY # list will be displayed joined with semi-colon
    "Full path to libprocps.a for static linkage"
    " leave empty for auto detection.")
  set(desc_PROCPS_STATIC_INCLUDE_DIR # displayed joined with semi-colon
    "Directory that contains proc/procps.h header for static linkage"
    " leave empty for auto detection.")
  set(libtype "STATIC")
  set(pc_vars_prefix ${pc}_STATIC)
  set(advanced_var_names "PROCPS_VERSION_STRING")
  set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_STATIC_LIBRARY_PREFIX})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})

else()
  set(desc_PROCPS_SHARED_LIBRARY # list will be displayed joined with semi-colon
    "Full path to libprocps.so for dynamic linkage"
    " leave empty for auto detection.")
  set(desc_PROCPS_SHARED_INCLUDE_DIR # displayed joined with semi-colon
    "Directory that contains proc/procps.h header for dynamic linkage"
    " leave empty for auto detection.")
  set(libtype "SHARED")
  set(pc_vars_prefix ${pc})
  set(advanced_var_names "PROCPS_INCLUDE_DIR" "PROCPS_LIBRARY"
                         "PROCPS_VERSION_STRING")
  set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_SHARED_LIBRARY_PREFIX})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()

## setup required headers
if(NOT PROCPS_HEADERS)
  set(PROCPS_HEADERS
              "proc/procps.h"
              "proc/sysinfo.h"
              "proc/pwcache.h"
              "proc/readproc.h" )
endif()


unset(found_lib)
unset(found_headers)

## be silent if nothing changed and previous result was ok
set(PROCPS_CONFIGURED OFF)
if (__procps_saved_OK 
    AND NOT DEFINED PROCPS_LIBRARY AND NOT DEFINED PROCPS_LIBRARY
    AND PROCPS_${libtype}_INCLUDE_DIR AND PROCPS_${libtype}_LIBRARY
    AND "${libtype}" STREQUAL "${__procps_saved_libtype}"
    AND "${PROCPS_HEADERS}" STREQUAL "${__procps_saved_HEADERS}"
    AND "${PROCPS_REQUIRE_NG}" STREQUAL "${__procps_saved_REQUIRE_NG}"
   )
  detect_changed_files("${libtype}")

  if(NOT PROCPS_CHANGED_LIBRARY AND NOT PROCPS_CHANGED_INCLUDE_DIR)
    set(PROCPS_CONFIGURED ON)
    set(found_headers TRUE)
    set(found_lib TRUE)
    msg_debug("Procps already configured.")
  endif()
endif()

set(__procps_saved_libtype "${libtype}" CACHE INTERNAL "")
set(__procps_saved_HEADERS "${PROCPS_HEADERS}" CACHE INTERNAL "")
set(__procps_saved_REQUIRE_NG "${PROCPS_REQUIRE_NG}" CACHE INTERNAL "")

# initial message
if (PROCPS_STATIC)
  msg("Checking for library procps and headers for static linking.")
else()
  msg("Checking for library procps and headers for dynamic linking.")
endif()

## setup require procps-ng
if(PROCPS_REQUIRE_NG)
  msg_verbose(
    "I: procps-ng required, other procps libraries will not be found.")
  set(log_libname "procps-ng")   # for logs
  set(lib_names "procps")        # for find_library()
  set(ver_rexp                   # regex of verion to be parsed from the lib 
    # 's/^\.\{0,\}procps-ng version \([0-9\.]\{1,\}\)\.\{0,\}$/\1/p'
    "s/^\\.\\{0,\\}procps-ng version \\([0-9\\.]\\{1,\\}\\)\\.\\{0,\\}$/\\1/p" )

else()
  set(log_libname "procps")      # for logs
  set(lib_names "procps" "proc") # for find_library()
  set(ver_rexp                   # regex of verion to be parsed from the lib
    # 's/^\.\{0,\}procps\(-ng\|\) version \([0-9\.]\{1,\}\)\.\{0,\}$/\2/p'
    "s/^\\.\\{0,\\}procps\\(-ng\\|\\) version \\([0-9\\.]\\{1,\\}\\)\\.\\{0,\\}$/\\2/p" )
endif()

# override PROCPS_${libtype}_INCLUDE_DIR with PROCPS_INCLUDE_DIR if passed
if(DEFINED PROCPS_INCLUDE_DIR)
  msg_verbose("I: Overriding PROCPS_${libtype}_INCLUDE_DIR with value from "
              "PROCPS_INCLUDE_DIR (\"${PROCPS_INCLUDE_DIR}\")")  
  set(PROCPS_${libtype}_INCLUDE_DIR ${PROCPS_INCLUDE_DIR}
      CACHE PATH "${desc_PROCPS_${libtype}_INCLUDE_DIR}" FORCE)
  msg_verbose("I: Unsetting PROCPS_INCLUDE_DIR variable.")
  unset(PROCPS_INCLUDE_DIR CACHE)
endif()

# overide PROCPS_${libtype}_LIBRARY with PROCPS_LIBRARY if set
if(DEFINED PROCPS_LIBRARY)
  msg_verbose("I: Overriding PROCPS_${libtype}_LIBRARY with value from "
              "PROCPS_LIBRARY (\"${PROCPS_LIBRARY}\")")
  set(PROCPS_${libtype}_LIBRARY "${PROCPS_LIBRARY}"
      CACHE FILEPATH "${desc_PROCPS_${libtype}_LIBRARY}" FORCE)
  msg_verbose("I: Unsetting PROCPS_LIBRARY variable.")
  unset(PROCPS_LIBRARY CACHE)
endif()

# -----------------------------------------------------------------------------
#  End setup
# -----------------------------------------------------------------------------


if(NOT PROCPS_CONFIGURED) # if configured skip finding procps
# -----------------------------------------------------------------------------
#  Begin finding procps library and headers
# -----------------------------------------------------------------------------


#  Query pkg-config
# -----------------------------------------------------------------------------

find_package(PkgConfig)
set(pc "__procps_pc") 
unset(${pc}_FOUND CACHE)
pkg_check_modules(${pc} QUIET libprocps)
if(NOT ${pc}_FOUND AND 
   NOT (PROCPS_${libtype}_LIBRARY AND PROCPS_${libtype}_INCLUDE_DIR)
  )
  msg_verbose("W: pkg-config failed to find libprocps.")
endif()


#  Find headers
# -----------------------------------------------------------------------------

# force find_path if user set PROCPS_(STATIC_)INCLUDE_DIR to empty string
if(NOT PROCPS_${libtype}_INCLUDE_DIR)
  unset(PROCPS_${libtype}_INCLUDE_DIR CACHE)
endif()

find_path(PROCPS_${libtype}_INCLUDE_DIR
          NAMES ${PROCPS_HEADERS}
          HINTS ${${pc_vars_prefix}_INCLUDEDIR}
                ${${pc_vars_prefix}_INCLUDE_DIRS}
          PATH_SUFFIXES libprocps procps
          DOC "${desc_PROCPS_${libtype}_INCLUDE_DIR}" )

if(NOT PROCPS_${libtype}_INCLUDE_DIR)
  error("Include directory of ${log_libname} library not found.")
else()
  msg_verbose("I: Include directory found: " ${PROCPS_${libtype}_INCLUDE_DIR})
  set(found_headers TRUE)
  foreach(header ${PROCPS_HEADERS})
    if(NOT EXISTS "${PROCPS_${libtype}_INCLUDE_DIR}/${header}")
      error("Header file \"${header}\" not found in include directory defined "
        "by PROCPS_${libtype}_INCLUDE_DIR variable "
        "('${PROCPS_${libtype}_INCLUDE_DIR}').")
      set(found_headers FALSE)
    else()
      msg_verbose("I: Header file \"${header}\" found.")
    endif()
  endforeach()
endif()


#  Find library
# -----------------------------------------------------------------------------

# force find_library if user set PROCPS_(STATIC_)LIBRARY to empty string
if(NOT PROCPS_${libtype}_LIBRARY)
  unset(PROCPS_${libtype}_LIBRARY CACHE)
endif()

find_library(PROCPS_${libtype}_LIBRARY NAMES ${lib_names}
             HINTS ${${pc_vars_prefix}_LIBDIR}
                   ${${pc_vars_prefix}_LIBRARY_DIRS}
                   DOC "${desc_PROCPS_${libtype}_LIBRARY}" )

if(NOT PROCPS_${libtype}_LIBRARY)
  error("Library ${log_libname} not found.")
elseif(NOT EXISTS ${PROCPS_${libtype}_LIBRARY})
  error("Library ${log_libname} defined in PROCPS_${libtype}_LIBRARY"
        " ('${PROCPS_${libtype}_LIBRARY}') not found.")
else()
  set(found_lib TRUE)
  msg_verbose("I: Library ${log_libname} found: " ${PROCPS_${libtype}_LIBRARY})
endif()


#  Find library version
# -----------------------------------------------------------------------------

if(found_lib)
  unset(PROCPS_${libtype}_VERSION_STRING CACHE) # check eachtime

  execute_process(
              COMMAND "strings" "${PROCPS_${libtype}_LIBRARY}"
              COMMAND "sed" "-n" "${ver_rexp}"
              OUTPUT_VARIABLE PROCPS_${libtype}_VERSION_STRING
              ERROR_QUIET )

  STRING(REGEX REPLACE "(\r?\n)+$" "" # trim EOL
         PROCPS_${libtype}_VERSION_STRING
         "${PROCPS_${libtype}_VERSION_STRING}")
  # store in cache
  set(PROCPS_${libtype}_VERSION_STRING
      "${PROCPS_${libtype}_VERSION_STRING}" CACHE INTERNAL "" )

  if(PROCPS_${libtype}_VERSION_STRING)
    msg_verbose("I: Version string of ${log_libname} found: "
                    "${PROCPS_${libtype}_VERSION_STRING}")
  else()
    set(found_lib FALSE)
    set(_err "Version string of ${log_libname} not found.")
    if(PROCPS_REQUIRE_NG)
      set(_err ${_err} " The found library may be legacy procps but"
                       " procps-ng is required.")
      error(${_err})
    endif()
  endif()
  
endif()


# -----------------------------------------------------------------------------
#  End finding procps library and headers
# -----------------------------------------------------------------------------

check_failed()

detect_changed_files(${libtype})


endif(NOT PROCPS_CONFIGURED)
# -----------------------------------------------------------------------------
# Store result
# -----------------------------------------------------------------------------

set(PROCPS_VERSION_STRING "${PROCPS_${libtype}_VERSION_STRING}")
set(PROCPS_LIBRARIES ${PROCPS_${libtype}_LIBRARY} )
set(PROCPS_INCLUDE_DIRS ${PROCPS_${libtype}_INCLUDE_DIR} )

if(     Procps_FIND_VERSION
        AND "${PROCPS_VERSION_STRING}" VERSION_LESS "${Procps_FIND_VERSION}"
   OR (
        Procps_FIND_VERSION AND Procps_FIND_VERSION_EXACT
        AND NOT "${PROCPS_VERSION_STRING}" VERSION_EQUAL "${Procps_FIND_VERSION}"
      )
  )
  finish()
endif()

mark_as_advanced(${advanced_var_names})

save_files_stat(${libtype}) # after this only symbols or struct members tests
                            # may fail 

# -----------------------------------------------------------------------------
#  Begin testing symbols
# -----------------------------------------------------------------------------

if(PROCPS_CHECK_SYMBOLS)
  set(CMAKE_REQUIRED_INCLUDES ${PROCPS_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${PROCPS_LIBRARIES})

  if(PROCPS_CHANGED_LIBRARY OR PROCPS_CHANGED_INCLUDE_DIR)
    foreach(_symbol ${PROCPS_CHECK_SYMBOLS})
      unset(__procps_${libtype}_HAS_${_symbol} CACHE)
    endforeach()
    msg_verbose("I: Checking libprocps symbols.")
  endif()

  unset(PROCPS_MISSING_SYMBOLS)
  unset(PROCPS_MISSING_REQUIRED_SYMBOLS)

  foreach(_symbol ${PROCPS_CHECK_SYMBOLS})
    unset(PROCPS_HAS_${_symbol} CACHE)

    if(DEFINED __procps_${libtype}_HAS_${_symbol})
      set(PROCPS_HAS_${_symbol} ${__procps_${libtype}_HAS_${_symbol}}
        CACHE INTERNAL "" )
    else()
      CHECK_SYMBOL_EXISTS("${_symbol}" "${PROCPS_HEADERS}"
              PROCPS_HAS_${_symbol} )
      set(__procps_${libtype}_HAS_${_symbol} ${PROCPS_HAS_${_symbol}}
          CACHE INTERNAL "" )
    endif()

    if(NOT PROCPS_HAS_${_symbol})
      set(PROCPS_MISSING_SYMBOLS "${PROCPS_MISSING_SYMBOLS} ${_symbol}" )
      foreach(_required_symbol ${PROCPS_REQUIRE_SYMBOLS})
        if(${_required_symbol} STREQUAL ${_symbol})
          set(PROCPS_MISSING_REQUIRED_SYMBOLS
              "${PROCPS_MISSING_REQUIRED_SYMBOLS} ${_symbol}" )
        endif()
      endforeach()
    endif()
  endforeach()

endif()

# -----------------------------------------------------------------------------
#  End testing symbols
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
#  Begin testing proc_t members
# -----------------------------------------------------------------------------

if(PROCPS_CHECK_PROC_T_MEMBERS)
  set(CMAKE_REQUIRED_INCLUDES ${PROCPS_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${PROCPS_LIBRARIES})

  if(PROCPS_CHANGED_LIBRARY OR PROCPS_CHANGED_INCLUDE_DIR)
    foreach(_member ${PROCPS_CHECK_PROC_T_MEMBERS})
      unset(__procps_${libtype}_PROC_T_HAS_MEMBER_${_member} CACHE)
    endforeach()
    msg_verbose("I: Checking libprocps proc_t members.")
  endif()

  unset(PROCPS_MISSING_PROC_T_MEMBERS)
  unset(PROCPS_MISSING_REQUIRED_PROC_T_MEMBERS)

  foreach(_member ${PROCPS_CHECK_PROC_T_MEMBERS})
    unset(PROCPS_PROC_T_HAS_MEMBER_${_member} CACHE)

    if(DEFINED __procps_${libtype}_PROC_T_HAS_MEMBER_${_member})
      set(PROCPS_PROC_T_HAS_MEMBER_${_member}
          ${__procps_${libtype}_PROC_T_HAS_MEMBER_${_member}}
          CACHE INTERNAL "" )
    else()
      CHECK_STRUCT_HAS_MEMBER("proc_t" "${_member}" "${PROCPS_HEADERS}"
            PROCPS_PROC_T_HAS_MEMBER_${_member} )
      set(__procps_${libtype}_PROC_T_HAS_MEMBER_${_member}
          ${PROCPS_PROC_T_HAS_MEMBER_${_member}} CACHE INTERNAL "" )
    endif()

    if(NOT PROCPS_PROC_T_HAS_MEMBER_${_member})
      set(PROCPS_MISSING_PROC_T_MEMBERS
          "${PROCPS_MISSING_PROC_T_MEMBERS} ${_member}" )
      foreach(_required_member ${PROCPS_REQUIRE_PROC_T_MEMBERS})
        if(${_required_member} STREQUAL ${_member})
          set(PROCPS_MISSING_REQUIRED_PROC_T_MEMBERS
              "${PROCPS_MISSING_REQUIRED_PROC_T_MEMBERS} ${_member}" )
        endif()
      endforeach()
    endif()
  endforeach()

endif()

# -----------------------------------------------------------------------------
#  End testing proc_t members
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
#  Begin outputing missing symbols and proc_t members.
# -----------------------------------------------------------------------------
# Send error and help message if required symbols and proc_t members not found.
# -----------------------------------------------------------------------------

if(PROCPS_MISSING_PROC_T_MEMBERS OR PROCPS_MISSING_SYMBOLS)
  if(PROCPS_MISSING_PROC_T_MEMBERS AND NOT Procps_FIND_QUIETLY)
    message(WARNING "libprocps struct proc_t missing members: "
            "${PROCPS_MISSING_PROC_T_MEMBERS}" )
  endif()
  if(PROCPS_MISSING_SYMBOLS AND NOT Procps_FIND_QUIETLY)
    message(WARNING "libprocps missing symbols: ${PROCPS_MISSING_SYMBOLS}")
  endif()
endif()

if(PROCPS_MISSING_REQUIRED_PROC_T_MEMBERS)
  if(NOT Procps_FIND_QUIETLY)
    message(WARNING "libprocps struct proc_t MISSING REQUIRED MEMBERS: "
            "${PROCPS_MISSING_REQUIRED_PROC_T_MEMBERS}" )
  endif()
  error("Required proc_t members missing in libprocps.")
endif()
if(PROCPS_MISSING_REQUIRED_SYMBOLS AND NOT Procps_FIND_QUIETLY)
  if(NOT Procps_FIND_QUIETLY)
    message(WARNING "libprocps MISSING REQUIRED SYMBOLS: "
            "${PROCPS_MISSING_REQUIRED_SYMBOLS}" )
  endif()
  error("Required symbols missing in libprocps.")
endif()

check_failed()

# -----------------------------------------------------------------------------
#  End outputing missing symbols and proc_t members.
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
#  Return results (code steps here only if confugiration was successfull)
# -----------------------------------------------------------------------------

set(__procps_saved_OK TRUE CACHE INTERNAL "")

finish()
