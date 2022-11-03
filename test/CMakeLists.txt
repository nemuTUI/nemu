if(NM_OS_LINUX)
    set(NEMU_TARGET_OS "LINUX")
elseif(NM_OS_FREEBSD)
    set(NEMU_TARGET_OS "FREEBSD")
endif()

add_custom_target(test
    DEPENDS nemu
    COMMENT "Run UI tests:"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND NEMU_BIN_DIR=${CMAKE_BINARY_DIR} QEMU_BIN_DIR=${NM_DEFAULT_QEMUDIR}
        NEMU_TARGET_OS=${NEMU_TARGET_OS} python -m unittest -v *_test.py
    )
