#!/bin/sh

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <linux|freebsd> <username>"
    exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "Run me as root" >&2
    exit 1
fi

KVM_GROUP=""
USB_GROUP=""
OS="$1"
USER="$2"

case "$OS" in
    ( linux )
        KVM_GROUP=$(ls -la /dev/kvm | cut -d ' ' -f 4)
        if [ "$KVM_GROUP" = "root" ]; then
            echo "Additional group for KVM device is missing" >&2
            exit 1
        fi

        USB_GROUP=$(ls -la /dev/bus/usb/001/001 | cut -d ' ' -f 4)
        if [ "$USB_GROUP" = "root" ]; then
            echo "Additional group for USB devices is missing" >&2
            exit 1
        fi

        if ! id -nG $USER | grep -qw $KVM_GROUP; then
          gpasswd -a $USER $KVM_GROUP
          [ "$?" -ne 0 ] && echo "[ERR]" && exit 1
        fi
        if ! id -nG $USER | grep -qw $USB_GROUP; then
          gpasswd -a $USER $USB_GROUP
          [ "$?" -ne 0 ] && echo "[ERR]" && exit 1
        fi

        ls -1 /usr/bin/qemu-system-* | xargs -n1 setcap CAP_NET_ADMIN=ep && \
        setcap CAP_NET_ADMIN=ep /usr/bin/nemu && \
        echo "[OK]"
        ;;

    ( * )
        echo "Unsupported" >&2
        ;;
esac
