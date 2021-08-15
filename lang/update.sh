#!/bin/bash

TMPDIR=$(mktemp -d /tmp/nemu_lang_XXXXXX)
[ $? != 0 ] && exit 1
trap "rm -rf ${TMPDIR}; rm -f nemu.pot; rm -f ru/nemu.po~" EXIT

pushd ../src > /dev/null

FILES=$(ls -1 *.c)

for F in $FILES; do
    E=${F//\.c/}
    gcc -I. -I/usr/include/json-c -I/usr/include/libxml2 \
        -I/usr/include/graphviz -I/usr/include/dbus-1.0  \
        -I/usr/lib64/dbus-1.0/include                    \
        -DNM_WITH_DBUS -DNM_WITH_NETWORK_MAP             \
        -DNM_WITH_OVF_SUPPORT -DNM_WITH_REMOTE           \
        -DNM_WITH_SPICE -DNM_WITH_VNC_CLIENT             \
        -E $F -o ${TMPDIR}/${E}.e
done

popd > /dev/null
xgettext --keyword=_ --language=C --add-comments --sort-output \
    --from-code=UTF-8 --omit-header -o nemu.pot $TMPDIR/*.e
msgmerge --update ru/nemu.po nemu.pot
