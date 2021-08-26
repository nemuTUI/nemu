macro(build_ncurses)
  include(ExternalProject)

  ExternalProject_Add(
    ncurses
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rd_party
    URL https://invisible-mirror.net/archives/ncurses/ncurses-6.2.tar.gz
    URL_MD5 e812da327b1c2214ac1aed440ea3ae8d
    CONFIGURE_COMMAND
      ${CMAKE_CURRENT_BINARY_DIR}/3rd_party/src/ncurses/configure
      --prefix=/
      --disable-db-install
      --enable-ext-colors
      --enable-ext-mouse
      --enable-widec
      --with-normal
      --without-ada
      --without-cxx
      --without-cxx-binding
      --without-develop
      --without-manpages
      --without-progs
      --without-tests
      $<$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>:--disable-leaks>
    INSTALL_COMMAND
      ${CMAKE_MAKE_PROGRAM}
      install
      DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/ncurses)

  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ncurses/include)

  add_library(libncurses STATIC IMPORTED)
  set_target_properties(
    libncurses PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/ncurses/lib/libncursesw.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/ncurses/include)
  add_dependencies(libncurses ncurses)

  add_library(libform STATIC IMPORTED)
  set_target_properties(
    libform PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/ncurses/lib/libformw.a)
  add_dependencies(libform libncurses)
  target_link_libraries(libform INTERFACE libncurses)

  add_library(libpanel STATIC IMPORTED)
  set_target_properties(
    libpanel PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/ncurses/lib/libpanelw.a)
  add_dependencies(libpanel libncurses)
  target_link_libraries(libform INTERFACE libncurses)

endmacro(build_ncurses)
