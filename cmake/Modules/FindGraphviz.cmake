if (GRAPHVIZ_LIBRARIES AND GRAPHVIZ_INCLUDE_DIRS)
  # in cache already
  set(GRAPHVIZ_FOUND TRUE)
else (GRAPHVIZ_LIBRARIES AND GRAPHVIZ_INCLUDE_DIRS)
  find_path(GRAPHVIZ_INCLUDE_DIR
    NAMES
  gvc.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
  PATH_SUFFIXES
    graphviz
  )

  find_library(GRAPHVIZ_GVC_LIBRARIE
      NAMES
        gvc
      PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    )

  find_library(GRAPHVIZ_CGRAPH_LIBRARIE
      NAMES
        cgraph
      PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    )

  set(GRAPHVIZ_INCLUDE_DIRS
      ${GRAPHVIZ_INCLUDE_DIR}
  )

  set(GRAPHVIZ_LIBRARIES
    ${GRAPHVIZ_GVC_LIBRARIE}
    ${GRAPHVIZ_CGRAPH_LIBRARIE}
  )

  if (GRAPHVIZ_INCLUDE_DIRS AND GRAPHVIZ_LIBRARIES)
    SET(GRAPHVIZ_FOUND "YES")
  endif(GRAPHVIZ_INCLUDE_DIRS AND GRAPHVIZ_LIBRARIES)

  if (GRAPHVIZ_FOUND)
      MESSAGE(STATUS "Found graphviz: ${GRAPHVIZ_LIBRARIES}")
      MESSAGE(STATUS " Includes: ${GRAPHVIZ_INCLUDE_DIRS}")
      MESSAGE(STATUS " Libraries: ${GRAPHVIZ_LIBRARIES}")
  else (GRAPHVIZ_FOUND)
      MESSAGE(STATUS "Graphviz not found.")
      if (Graphviz_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find Graphviz libraries")
      endif (Graphviz_FIND_REQUIRED)
  endif (GRAPHVIZ_FOUND)

endif (GRAPHVIZ_LIBRARIES AND GRAPHVIZ_INCLUDE_DIRS)
