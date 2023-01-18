#!/bin/sh

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <database_path> <qemu_bin_dir>" >&2
    exit 1
fi

DB_PATH="$1"
DB_ACTUAL_VERSION=20
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
    #TODO we shouldn't expect to find .nemu.cfg in homedir
    if [ ! -f ${USER_DIR}/.nemu.cfg ]; then
        echo "Couldn't find .nemu.cfg in user home directory" >&2
        exit 1
    fi

    #TODO we shouldn't expect to find .nemu.cfg in homedir
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

db_upgrade_snaps()
{
    oIFS="$IFS"
    IFS=$'\n'
    local rc=0

    for n in $(sqlite3 "$DB_PATH" --list 'SELECT * FROM vmsnapshots_tmp'); do
        local vm_name=$(echo $n | cut -d '|' -f 2)
        local sn_name=$(echo $n | cut -d '|' -f 3)
        local load=$(echo $n | cut -d '|' -f 4)
        local ts=$(echo $n | cut -d '|' -f 5)
        local vm_id=$(sqlite3 "$DB_PATH" --quote "SELECT id FROM vms WHERE name='"${vm_name}"'")

        sqlite3 "$DB_PATH" --line "PRAGMA foreign_keys=ON; INSERT INTO vmsnapshots "`
            `"(snap_name, load, timestamp, vm_id) VALUES "`
            `"('"${sn_name}"',"${load}",'"${ts}"',"${vm_id}")" || rc=1
    done

    IFS="$oIFS"
    return $rc
}

db_upgrade_usb()
{
    oIFS="$IFS"
    IFS=$'\n'
    local rc=0

    for n in $(sqlite3 "$DB_PATH" --list 'SELECT * FROM usb_tmp'); do
        local vm_name=$(echo $n | cut -d '|' -f 2)
        local dev_name=$(echo $n | cut -d '|' -f 3)
        local vendor_id=$(echo $n | cut -d '|' -f 4)
        local product_id=$(echo $n | cut -d '|' -f 5)
        local serial=$(echo $n | cut -d '|' -f 6)
        local vm_id=$(sqlite3 "$DB_PATH" --quote "SELECT id FROM vms WHERE name='"${vm_name}"'")

        sqlite3 "$DB_PATH" --line "PRAGMA foreign_keys=ON; INSERT INTO usb "`
            `"(dev_name, vendor_id, product_id, serial, vm_id) VALUES "`
            `"('"${dev_name}"',"${vendor_id}",'"${product_id}"','"${serial}"',"${vm_id}")" || rc=1
    done

    IFS="$oIFS"
    return $rc
}

db_upgrade_drive()
{
    oIFS="$IFS"
    IFS=$'\n'
    local rc=0

    for n in $(sqlite3 "$DB_PATH" --list 'SELECT * FROM drives_tmp'); do
        local vm_name=$(echo $n | cut -d '|' -f 2)
        local drive_name=$(echo $n | cut -d '|' -f 3)
        local drive_drv=$(echo $n | cut -d '|' -f 4)
        local capacity=$(echo $n | cut -d '|' -f 5)
        local boot=$(echo $n | cut -d '|' -f 6)
        local discard=$(echo $n | cut -d '|' -f 7)
        local vm_id=$(sqlite3 "$DB_PATH" --quote "SELECT id FROM vms WHERE name='"${vm_name}"'")

        sqlite3 "$DB_PATH" --line "PRAGMA foreign_keys=ON; INSERT INTO drives "`
            `"(drive_name, drive_drv, capacity, boot, discard, vm_id) VALUES "`
            `"('"${drive_name}"','"${drive_drv}"',"${capacity}","${boot}","${discard}","${vm_id}")" || rc=1
    done

    IFS="$oIFS"
    return $rc
}

db_upgrade_ifs()
{
    oIFS="$IFS"
    IFS=$'\n'
    local rc=0

    for n in $(sqlite3 "$DB_PATH" --list 'SELECT * FROM ifaces_tmp'); do
        local vm_name=$(echo $n | cut -d '|' -f 2)
        local if_name=$(echo $n | cut -d '|' -f 3)
        local mac_addr=$(echo $n | cut -d '|' -f 4)
        local ipv4_addr=$(echo $n | cut -d '|' -f 5)
        local if_drv=$(echo $n | cut -d '|' -f 6)
        local vhost=$(echo $n | cut -d '|' -f 7)
        local macvtap=$(echo $n | cut -d '|' -f 8)
        local parent_eth=$(echo $n | cut -d '|' -f 9)
        local altname=$(echo $n | cut -d '|' -f 10)
        local netuser=$(echo $n | cut -d '|' -f 11)
        local hostfwd=$(echo $n | cut -d '|' -f 12)
        local smb=$(echo $n | cut -d '|' -f 13)
        local vm_id=$(sqlite3 "$DB_PATH" --quote "SELECT id FROM vms WHERE name='"${vm_name}"'")

        sqlite3 "$DB_PATH" --line "PRAGMA foreign_keys=ON; INSERT INTO ifaces "`
            `"(if_name, mac_addr, if_drv, vhost, macvtap, netuser, vm_id, ipv4_addr, "`
            `"parent_eth, hostfwd, smb, altname"`
            `") VALUES "`
            `"('"${if_name}"','"${mac_addr}"','"${if_drv}"',"${vhost}","${macvtap}",`
            `"${netuser}","${vm_id}",'"${ipv4_addr}"','"${parent_eth}"','"${hostfwd}"',`
            `'"${smb}"','"${altname}"')" || rc=1
    done

    IFS="$oIFS"
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
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=14'
            ) || RC=1
            ;;

        ( 14 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE drives ADD discard integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE drives SET discard="0";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=15'
            ) || RC=1
            ;;

        ( 15 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD netuser integer;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD hostfwd char;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE ifaces SET netuser="0";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=16'
            ) || RC=1
            ;;

        ( 16 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces ADD smb char;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=17'
            ) || RC=1
           ;;

        ( 17 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD pflash char;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=18'
            ) || RC=1
            ;;

        ( 18 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD spice_agent integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET spice_agent="0";' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=19'
            ) || RC=1
            ;;

        ( 19 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms RENAME TO vms_tmp;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vmsnapshots RENAME TO vmsnapshots_tmp;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE drives RENAME TO drives_tmp;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE ifaces RENAME TO ifaces_tmp;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE usb RENAME TO usb_tmp;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE veth RENAME TO veth_tmp;' &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE vms(id INTEGER NOT NULL PRIMARY KEY, '`
               `'name TEXT NOT NULL, mem INTEGER NOT NULL, smp TEXT NOT NULL, '`
               `'kvm INTEGER NOT NULL, hcpu INTEGER NOT NULL, vnc INTEGER NOT NULL, '`
               `'arch TEXT NOT NULL, iso TEXT, install INTEGER NOT NULL, '`
               `'usb INTEGER NOT NULL, usb_status INTEGER NOT NULL, bios TEXT, '`
               `'kernel TEXT, mouse_override INTEGER NOT NULL, kernel_append TEXT, '`
               `'tty_path TEXT, socket_path TEXT, initrd TEXT, machine TEXT NOT NULL, '`
               `'fs9p_enable INTEGER NOT NULL, fs9p_path TEXT, fs9p_name TEXT, '`
               `'usb_type TEXT NOT NULL, spice INTEGER NOT NULL, debug_port INTEGER, '`
               `'debug_freeze INTEGER NOT NULL, cmdappend TEXT, team TEXT, '`
               `'display_type TEXT NOT NULL, pflash TEXT, spice_agent INTEGER NOT NULL);' &&
            sqlite3 "$DB_PATH" -line 'INSERT INTO vms SELECT * FROM vms_tmp;' &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE vmsnapshots(snap_name TEXT NOT NULL, '`
               `'load INTEGER NOT NULL, timestamp TEXT NOT NULL, vm_id INTEGER NOT NULL, '`
               `'FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE);' &&
            db_upgrade_snaps &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE ifaces(if_name TEXT NOT NULL, '`
               `'mac_addr TEXT NOT NULL, ipv4_addr TEXT, if_drv TEXT NOT NULL, '`
               `'vhost INTEGER NOT NULL, macvtap INTEGER NOT NULL, parent_eth TEXT, '`
               `'altname TEXT, netuser INTEGER NOT NULL, hostfwd TEXT, smb TEXT, '`
               `'vm_id INTEGER NOT NULL, '`
               `'FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE);' &&
            db_upgrade_ifs &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE usb(dev_name TEXT NOT NULL, '`
               `'vendor_id TEXT NOT NULL, product_id TEXT NOT NULL, '`
               `'serial TEXT NOT NULL, vm_id INTEGER NOT NULL, '`
               `'FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE);' &&
            db_upgrade_usb &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE drives(drive_name TEXT NOT NULL, '`
               `'drive_drv TEXT NOT NULL, capacity INTEGER NOT NULL, boot INTEGER NOT NULL, '`
               `'discard INTEGER NOT NULL, vm_id INTEGER NOT NULL, '`
               `'FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE);' &&
            db_upgrade_drive &&
            sqlite3 "$DB_PATH" -line 'CREATE TABLE veth(l_name TEXT NOT NULL, '`
               `'r_name char TEXT NOT NULL)' &&
            sqlite3 "$DB_PATH" -line 'INSERT INTO veth (l_name, r_name) '`
               `'SELECT l_name, r_name FROM veth_tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE vmsnapshots_tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE ifaces_tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE drives_tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE veth_tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE usb_tmp;' &&
            sqlite3 "$DB_PATH" -line 'DROP TABLE vms_tmp;' &&
            sqlite3 "$DB_PATH" -line "CREATE TRIGGER clear_deleted_veth AFTER DELETE ON veth "`
               `"BEGIN UPDATE ifaces SET macvtap='0', parent_eth='' WHERE parent_eth=old.l_name "`
               `"OR parent_eth=old.r_name; END;" &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=20'
            ) || RC=1
            ;;

        ( * )
            echo "Unsupported database user_version" >&2
            exit 1
    esac

    DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*[[:space:]]=[[:space:]]//')
done

[ "$RC" = 0 ] && echo "database upgrade complete."
