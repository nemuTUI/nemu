#!/bin/bash

COMP_DIR="/usr/share/bash-completion/completions"

if [ $1 == 0 ] && [ -d "$COMP_DIR" ]; then
  if [ -f "$COMP_DIR/nemu" ]; then
    echo "Uninstalling bash completion script ${COMP_DIR}/nemu"
    unlink "${COMP_DIR}/nemu"
  fi
fi
