#!/bin/sh

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <database_path> <qemu_bin_dir>" >&2
    exit 1
fi

DB_PATH="$1"
DB_ACTUAL_VERSION=14
DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*[[:space:]]=[[:space:]]//')
USER=$(whoami)
RC=0

if [ "$#" -eq 2 ]; then
    QEMU_BIN_PATH="$3"
else
    USER_DIR=$(grep ${USER} /etc/passwd | awk 'BEGIN { FS = ":" }; { printf "%s\n", $6 }')
    if [ ! -d "$USER_DIR" ]; then
        echo "Couldn't find user home directory" >&2
        exit 1
    fi
    if [ ! -f ${USER_DIR}/.nemu.cfg ]; then
        echo "Couldn't find .nemu.cfg in user home directory" >&2
        exit 1
    fi

    QEMU_BIN_PATH=$(grep qemu_bin_path ${USER_DIR}/.nemu.cfg | awk '{ printf "%s\n", $3 }')
    if [ -z "$QEMU_BIN_PATH" ]; then
        echo "Couldn't get qemu_bin_path from .nemu.cfg" >&2
        exit 1
    fi
fi

if [ "$DB_CURRENT_VERSION" -lt 3 ]; then
    echo "Database version less then 3 is not supported"
    exit 1
fi

if [ "$DB_CURRENT_VERSION" = "$DB_ACTUAL_VERSION" ]; then
    echo "No need to upgrade."
    exit 0
fi

db_update_machine()
{
# TODO make machine column NOT NULL
    local a
    local rc=0
    local archs=$(sqlite3 "$DB_PATH" -line 'SELECT DISTINCT arch FROM vms;' | awk '{print $NF}')

    for a in $archs; do
        local def=$(${QEMU_BIN_PATH}/qemu-system-"${a}" -M help | awk '/(default)/ {print $1}')
        [ -z "$def" ] && rc=1
        sqlite3 "$DB_PATH" -line "UPDATE vms SET machine='${def}' WHERE machine IS NULL and arch='${a}'" || rc=1
    done

    return $rc
}

echo "database version: ${DB_CURRENT_VERSION}"
while [ "$DB_CURRENT_VERSION" != "$DB_ACTUAL_VERSION" ]; do
    [ "$RC" = 1 ] && break;

    case "$DB_CURRENT_VERSION" in
        ( 3 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD initrd char;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD machine char;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=4'
            ) || RC=1
            ;;

        ( 4 )
            (
            sqlite3 "$DB_PATH" -line 'CREATE TABLE snapshots(id integer primary key autoincrement, '`
               `'vm_name char, snap_name char, backing_drive char, snap_idx integer, '`
               `'active integer, TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL)' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD vhost integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE ifaces SET vhost="0";' &&
            sqlite3 "$DB_PATH" -line 'UPDATE ifaces SET if_drv="virtio-net-pci" WHERE if_drv="virtio";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=5'
            ) || RC=1
            ;;

        ( 5 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD fs9p_enable integer;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD fs9p_path char;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD fs9p_name char;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET fs9p_enable="0";' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD macvtap integer;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD parent_eth char;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE ifaces SET macvtap="0";' &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE veth(id integer primary key autoincrement, '`
                `'l_name char, r_name char);' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=6'
            ) || RC=1
            ;;

        ( 6 )
            (
            sqlite3 "$DB_PATH" -line 'CREATE TABLE vmsnapshots(id integer primary key autoincrement, '`
               `'vm_name char, snap_name char, load integer, timestamp char)' &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE usb(id integer primary key autoincrement, '`
                `'vm_name char, dev_name char, vendor_id char, product_id char, serial char)' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=7'
            ) || RC=1
            ;;

        ( 7 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD usb_type char;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET usb_type="XHCI";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=8'
            ) || RC=1
            ;;

        ( 8 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD spice integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET spice="1";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=9'
            ) || RC=1
            ;;

        ( 9 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD debug_port integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET debug_port="";' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD debug_freeze integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET debug_freeze="0";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=10'
            ) || RC=1
            ;;

        ( 10 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD cmdappend char;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD altname char;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=11'
            ) || RC=1
            ;;

         ( 11 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD team char;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE IF EXISTS lastval;' &&
            db_update_machine &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=12'
            ) || RC=1
            ;;

         ( 12 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms RENAME TO tmp;' &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE vms(id integer PRIMARY KEY AUTOINCREMENT, '`
               `'name char(31), mem integer, smp char, kvm integer, '`
               `'hcpu integer, vnc integer, arch char(32), iso char, '`
               `'install integer, usb integer, usbid char, bios char, kernel char, '`
               `'mouse_override integer, kernel_append char, tty_path char, '`
               `'socket_path char, initrd char, machine char, fs9p_enable integer, '`
               `'fs9p_path char, fs9p_name char, usb_type char, spice integer, '`
               `'debug_port integer, debug_freeze integer, cmdappend char, team char);' &&
            sqlite3 "$DB_PATH" -line 'INSERT INTO vms SELECT * FROM tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE tmp;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=13'
            ) || RC=1
            ;;
         ( 13 )
         (
             sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD display_type char;' &&
             sqlite3 "$DB_PATH" -line 'UPDATE vms SET display_type="qxl";' &&
             sqlite3 "$DB_PATH" -line 'PRAGMA user_version=13'
         ) || RC=1
            ;;
        ( * )
            echo "Unsupported database user_version" >&2
            exit 1
    esac

    DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*[[:space:]]=[[:space:]]//')
done

[ "$RC" = 0 ] && echo "database upgrade complete."
