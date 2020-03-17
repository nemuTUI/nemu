#!/bin/sh

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 <linux|freebsd> <username> <qemu_bin_dir>"
    exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "Run me as root" >&2
    exit 1
fi

KVM_GROUP=""
USB_GROUP=""
VHOST_GROUP=""
OS="$1"
USER="$2"

if [ "$#" -eq 3 ]; then
    QEMU_BIN_PATH="$3"
else
    USER_DIR=$(grep ${USER} /etc/passwd | awk 'BEGIN { FS = ":" }; { printf "%s\n", $6 }')
    if [ ! -d $USER_DIR ]; then
        echo "Couldn't find user home directory" >&2
        exit 1
    fi
    if [ ! -f $USER_DIR/.nemu.cfg ]; then
        echo "Couldn't find .nemu.cfg in user home directory" >&2
        exit 1
    fi

    QEMU_BIN_PATH=$(grep qemu_bin_path ${USER_DIR}/.nemu.cfg | awk '{ printf "%s\n", $3 }')
    if [ -z$ QEMU_BIN_PATH ]; then
        echo "Couldn't get qemu_bin_path from .nemu.cfg" >&2
        exit 1
    fi
fi

case "$OS" in
    ( linux )
        KVM_GROUP=$(ls -la /dev/kvm | cut -d ' ' -f 4)
        if [ "$KVM_GROUP" = "root" ]; then
            echo "Warning: Additional group for KVM device is missing" >&2
            echo "Fix it and run script again, \"-enable-kvm\" will not work" >&2
        else
          if ! id -nG $USER | grep -qw $KVM_GROUP; then
            gpasswd -a $USER $KVM_GROUP
            [ "$?" -ne 0 ] && echo "[ERR]" && exit 1
          fi
        fi

        VHOST_GROUP=$(ls -la /dev/vhost-net | cut -d ' ' -f 4)
        if [ "$VHOST_GROUP" = "root" ]; then
            echo "Warning: Additional group for KVM vhost-net device is missing" >&2
            echo "Fix it and run script again, \"vhost=on\" will not work" >&2
        fi

        USB_GROUP=$(ls -la /dev/bus/usb/001/001 | cut -d ' ' -f 4)
        if [ "$USB_GROUP" = "root" ]; then
            echo "Warning: additional group for USB devices is missing" >&2
            echo "Fix it and run script again, \"-usb\" will not work" >&2
        else
          if ! id -nG $USER | grep -qw $USB_GROUP; then
            gpasswd -a $USER $USB_GROUP
            [ "$?" -ne 0 ] && echo "[ERR]" && exit 1
          fi
        fi

        ls -1 $QEMU_BIN_PATH/qemu-system-* | xargs -n1 setcap CAP_NET_ADMIN=ep && \
        setcap CAP_NET_ADMIN=ep /usr/bin/nemu && \
        echo "[OK]"
        ;;

    ( * )
        echo "Unsupported" >&2
        ;;
esac
