#!/bin/bash

NEMU_COMP_SCRIPT="/usr/share/nemu/scripts/nemu.bash"
COMP_DIR="/usr/share/bash-completion/completions"

if [ -d "$COMP_DIR" ]; then
  if [ ! -f "${COMP_DIR}/nemu" ]; then
    echo "Installing bash completion script in ${COMP_DIR}"
    ln -s "${NEMU_COMP_SCRIPT}" "${COMP_DIR}/nemu"
  fi
fi