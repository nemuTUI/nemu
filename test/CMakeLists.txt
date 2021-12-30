add_custom_target(test
    DEPENDS nemu
    COMMENT "Run UI tests:"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND NEMU_BIN_DIR=${CMAKE_BINARY_DIR} QEMU_BIN_DIR=${NM_DEFAULT_QEMUDIR} python -m tap *_test.py
    )
