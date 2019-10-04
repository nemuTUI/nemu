#!/bin/bash

if ! which git > /dev/null 2>&1; then
  exit 0
fi

cd "$1"

if ! git describe > /dev/null 2>&1; then
  exit 0
fi

echo -n "$(git describe --always --tags)"
