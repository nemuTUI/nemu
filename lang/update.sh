#!/bin/bash

TMPDIR=$(mktemp -d /tmp/nemu_lang_XXXXXX)
[ $? != 0 ] && exit 1
trap "rm -rf ${TMPDIR}; rm -f nemu.pot; rm -f ru/nemu.po~" EXIT

pushd ../src > /dev/null

FILES=$(ls -1 *.c)

# xgettext doesn't recognise this:
#
# #define MSG "message"
# printf("%s", _(msg));
#
# so we feed it with postprocessed files.
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

pushd $TMPDIR > /dev/null

# xgettext doesn't recognise this:
#
# static const char msg[] = "message";
# printf("%s", _(msg));
#
# so we have to edit code. We use NM_LC_ prefix for strings 
# that must be processed by gettext.
for F in $FILES; do
    E=${F//\.c/\.e}
    sed -ri 's/(^static const char NM_LC_.*= )(".*")(;$)/\1_(\2_)\3/' $E
done

popd > /dev/null
popd > /dev/null
xgettext --keyword=_ --language=C --add-comments --sort-output \
    --from-code=UTF-8 --omit-header -o nemu.pot $TMPDIR/*.e
msgmerge -N --update ru/nemu.po nemu.pot
