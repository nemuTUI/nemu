#!/bin/bash
set -eu

OVA_URL='https://drive.google.com/u/0/uc?id=1vxpcgv_hR4UpIoK9QvPeXC22BnuZYK3n'

test -d testdata || mkdir testdata

pushd testdata > /dev/null
if ! test -f dsl.ova; then
    gdown "${OVA_URL}"
fi
popd > /dev/null

python3 -m unittest -v *_test.py
