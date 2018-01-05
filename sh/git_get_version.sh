#!/bin/sh

if ! which git &>/dev/null; then
  exit 0
fi

cd "$1"

if ! git describe &>/dev/null; then
  exit 0
fi

echo -n "\"$(git describe --abbrev=4 --always --tags)\""
