#configure env
cmake_minimum_required(VERSION 2.8)
project(qemu-manage)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)



#find packcages
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(Procps REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS regex)

#configure project
include_directories(${CURSES_INCLUDE_DIR}
                    ${SQLITE3_INCLUDE_DIR}
                    ${PROCPS_INCLUDE_DIRS}
                    ${Boost_INCLUDE_DIRS})

aux_source_directory(src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}
                    ${CURSES_LIBRARIES}
                    ${SQLITE3_LIBRARIES}
                    ${PROCPS_LIBRARIES}
                    ${CMAKE_THREAD_LIBS_INIT}
                    ${Boost_LIBRARIES})

#uncomment line below to enable Open vSwitch support
#add_definitions(-DENABLE_OPENVSWITCH)

#set path
set(USR_PREFIX "/usr" CACHE PATH "Executable and lang files directory prefix")
set(ETC_PREFIX "/etc" CACHE PATH "Config file Installation directory prefix")
add_definitions(-DUSR_PREFIX=${USR_PREFIX})
add_definitions(-DETC_PREFIX=${ETC_PREFIX})

add_subdirectory(lang)
add_dependencies(${PROJECT_NAME} lang)

#configure install
set(QEMU_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.cfg.sample")
install(TARGETS ${PROJECT_NAME} DESTINATION ${USR_PREFIX}/bin)
install(FILES ${QEMU_CONFIG_SOURCE} DESTINATION ${ETC_PREFIX} RENAME ${PROJECT_NAME}.cfg)

#x11 check and flags setup
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
        set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
        set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++0x")
else()
        message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()
