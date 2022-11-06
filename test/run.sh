#!/bin/bash
set -eu

pushd testdata > /dev/null
if ! test -f deb_a.img; then
    tar xvf deb_a.img.tar.xz
fi
popd > /dev/null

python3 -m unittest -v *_test.py
