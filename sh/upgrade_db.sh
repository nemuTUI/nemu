#!/bin/sh

if [ -z "$1" ]; then
    echo "Usage: $0 <database_path>" >&2
    exit 1
fi

DB_PATH="$1"
DB_ACTUAL_VERSION=10
DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*[[:space:]]=[[:space:]]//')
RC=0

if [ "$DB_CURRENT_VERSION" -lt 3 ]; then
    echo "Database version less then 3 is not supported"
    exit 1
fi

if [ "$DB_CURRENT_VERSION" = "$DB_ACTUAL_VERSION" ]; then
    echo "No need to upgrade."
    exit 0
fi

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

        ( * )
            echo "Unsupported database user_version" >&2
            exit 1
    esac
    
    DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*[[:space:]]=[[:space:]]//')
done

[ "$RC" = 0 ] && echo "database upgrade complete."
