#ifndef NM_DATABASE_H_
#define NM_DATABASE_H_

#include <nm_vector.h>
#include <stdbool.h>

#include <sqlite3.h>

#define NM_DB_VERSION "22"

/* PRAGMAS */
static const char NM_SQL_PRAGMA_FOREIGN_ON[] =
    "PRAGMA foreign_keys=ON";
static const char NM_SQL_PRAGMA_USER_VERSION_GET[] =
    "PRAGMA user_version";
static const char NM_SQL_PRAGMA_USER_VERSION_SET[] =
    "PRAGMA user_version=" NM_DB_VERSION;

/* CREATE */
static const char NM_SQL_VMS_CREATE[] =
    "CREATE TABLE vms(id INTEGER NOT NULL PRIMARY KEY, "
    "name TEXT NOT NULL, mem INTEGER NOT NULL, smp TEXT NOT NULL, "
    "kvm INTEGER NOT NULL, hcpu INTEGER NOT NULL, vnc INTEGER NOT NULL, "
    "arch TEXT NOT NULL, iso TEXT, install INTEGER NOT NULL, "
    "usb INTEGER NOT NULL, usb_status INTEGER NOT NULL, bios TEXT, "
    "kernel TEXT, mouse_override INTEGER NOT NULL, kernel_append TEXT, "
    "tty_path TEXT, socket_path TEXT, initrd TEXT, machine TEXT NOT NULL, "
    "fs9p_enable INTEGER NOT NULL, fs9p_path TEXT, fs9p_name TEXT, "
    "usb_type TEXT NOT NULL, spice INTEGER NOT NULL, debug_port INTEGER, "
    "debug_freeze INTEGER NOT NULL, cmdappend TEXT, team TEXT, "
    "display_type TEXT NOT NULL, pflash TEXT, spice_agent INTEGER NOT NULL)";

static const char NM_SQL_IFACES_CREATE[] =
    "CREATE TABLE ifaces(if_name TEXT NOT NULL, "
    "mac_addr TEXT NOT NULL, ipv4_addr TEXT, if_drv TEXT NOT NULL, "
    "vhost INTEGER NOT NULL, macvtap INTEGER NOT NULL, parent_eth TEXT, "
    "altname TEXT, netuser INTEGER NOT NULL, hostfwd TEXT, smb TEXT, "
    "bridge INTEGER DEFAULT 0, bridge_eth TEXT, "
    "vm_id INTEGER NOT NULL, "
    "FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE)";

static const char NM_SQL_DRIVES_CREATE[] =
    "CREATE TABLE drives(drive_name TEXT NOT NULL, drive_drv TEXT NOT NULL, "
    "capacity INTEGER NOT NULL, boot INTEGER NOT NULL, "
    "discard INTEGER NOT NULL, vm_id INTEGER NOT NULL, "
    "format TEXT NOT NULL, "
    "FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE)";

static const char NM_SQL_SNAPS_CREATE[] =
    "CREATE TABLE vmsnapshots(snap_name TEXT NOT NULL, "
    "load INTEGER NOT NULL, timestamp TEXT NOT NULL, vm_id INTEGER NOT NULL, "
    "FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE)";

static const char NM_SQL_USB_CREATE[] =
    "CREATE TABLE usb(dev_name TEXT NOT NULL, "
    "vendor_id TEXT NOT NULL, product_id TEXT NOT NULL, "
    "serial TEXT NOT NULL, vm_id INTEGER NOT NULL, "
    "FOREIGN KEY(vm_id) REFERENCES vms(id) ON DELETE CASCADE)";

static const char NM_SQL_VETH_CREATE[] =
    "CREATE TABLE veth(l_name TEXT NOT NULL, r_name char TEXT NOT NULL)";

static const char NM_SQL_VETH_CREATE_TRIGGER[] =
    "CREATE TRIGGER clear_deleted_veth AFTER DELETE ON veth "
    "BEGIN UPDATE ifaces SET macvtap='0', parent_eth='' "
    "WHERE parent_eth=old.l_name OR parent_eth=old.r_name; END";

/* VMS */
static const char NM_SQL_VMS_INSERT_CLONE[] =
    "INSERT INTO vms SELECT NULL, '%s', mem, smp, kvm, hcpu, %d, arch, "
    "iso, install, usb, usb_status, bios, kernel, mouse_override, "
    "kernel_append, tty_path, socket_path, initrd, machine, fs9p_enable, "
    "fs9p_path, fs9p_name, usb_type, spice, debug_port, debug_freeze, "
    "cmdappend, team, display_type, pflash, spice_agent "
    "FROM vms WHERE name='%s'";

static const char NM_SQL_VMS_INSERT_NEW[] =
    "INSERT INTO vms(name, mem, smp, kvm, hcpu, vnc, arch, iso, "
    "install, machine, mouse_override, usb, usb_type, usb_status, fs9p_enable, "
    "spice, debug_port, debug_freeze, display_type, spice_agent) "
    "VALUES('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', "
    "'%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', %s)";

static const char NM_SQL_VMS_SELECT_ID[] =
    "SELECT id FROM vms WHERE name='%s'";

static const char NM_SQL_VMS_SELECT_ALL[] =
    "SELECT * FROM vms WHERE name='%s'";

static const char NM_SQL_VMS_SELECT_FREE_VNC[] =
    "SELECT DISTINCT vnc + 1 FROM vms "
    "UNION SELECT 0 EXCEPT SELECT DISTINCT vnc FROM vms "
    "ORDER BY vnc ASC LIMIT 1";

static const char NM_SQL_VMS_SELECT_ALL_VNC[] =
    "SELECT vnc FROM vms ORDER BY vnc ASC";

static const char NM_SQL_VMS_SELECT_VNC[] =
    "SELECT vnc, spice FROM vms WHERE name='%s'";

static const char NM_SQL_VMS_SELECT_NAMES[] =
    "SELECT name FROM vms ORDER BY name ASC";

static const char NM_SQL_VMS_SELECT_NAMES_BY_TEAM[] =
    "SELECT name FROM vms WHERE team='%s' ORDER BY name ASC";

static const char NM_SQL_VMS_SELECT_PROPS[] =
    "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost, "
    "macvtap, parent_eth, altname, netuser, bridge, bridge_eth, "
    "hostfwd, smb FROM ifaces "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s') ORDER BY if_name ASC";

static const char NM_SQL_VMS_SELECT_PROPS_BY_ID[] =
    "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost, "
    "macvtap, parent_eth, altname, netuser, bridge, bridge_eth, "
    "hostfwd, smb FROM ifaces "
    "WHERE vm_id=%s ORDER BY if_name ASC";

static const char NM_SQL_VMS_SELECT_TEAMS[] =
    "SELECT DISTINCT team FROM vms WHERE team <> \"\"";

static const char NM_SQL_VMS_UPDATE_VNC[] =
    "UPDATE vms SET vnc=%u WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_SPICE[] =
    "UPDATE vms SET spice=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_NAME[] =
    "UPDATE vms SET name='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_SMP[] =
    "UPDATE vms SET smp='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_TTY[] =
    "UPDATE vms SET tty_path='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_SOCKET[] =
    "UPDATE vms SET socket_path='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_MEM[] =
    "UPDATE vms SET mem=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_KVM[] =
    "UPDATE vms SET kvm=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_MOUSE[] =
    "UPDATE vms SET mouse_override=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_AGENT[] =
    "UPDATE vms SET spice_agent=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_HCPU[] =
    "UPDATE vms SET hcpu=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_USB[] =
    "UPDATE vms SET usb=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_INSTALL[] =
    "UPDATE vms SET install=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_DBG_PORT[] =
    "UPDATE vms SET debug_port=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_DBG_FREEZE[] =
    "UPDATE vms SET debug_freeze=%s WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_INITRD[] =
    "UPDATE vms SET initrd='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_ISO[] =
    "UPDATE vms SET iso='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_BIOS[] =
    "UPDATE vms SET bios='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_FLASH[] =
    "UPDATE vms SET pflash='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_KERN[] =
    "UPDATE vms SET kernel='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_KERN_CMD[] =
    "UPDATE vms SET kernel_append='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_USBTYPE[] =
    "UPDATE vms SET usb_type='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_DISPLAY[] =
    "UPDATE vms SET display_type='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_MACHINE[] =
    "UPDATE vms SET machine='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_CMD[] =
    "UPDATE vms SET cmdappend='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_TEAM[] =
    "UPDATE vms SET team='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_9P_MODE[] =
    "UPDATE vms SET fs9p_enable='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_9P_PATH[] =
    "UPDATE vms SET fs9p_path='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_9P_NAME[] =
    "UPDATE vms SET fs9p_name='%s' WHERE name='%s'";

static const char NM_SQL_VMS_UPDATE_USB_STATUS[] =
    "UPDATE vms SET usb_status=%s WHERE name='%s'";

static const char NM_SQL_VMS_DELETE_VM[] =
    "DELETE FROM vms WHERE name='%s'";

/* IFACES */
static const char NM_SQL_IFACES_INSERT_CLONED[] =
    "INSERT INTO ifaces(vm_id, if_name, mac_addr, if_drv, vhost, "
    "macvtap, parent_eth, altname, netuser, hostfwd, smb) "
    "VALUES ((SELECT id FROM vms WHERE name='%s'), "
    "'%s', '%s', '%s', %s, %s, '%s', '%s', %s, '%s', '%s')";

static const char NM_SQL_IFACES_INSERT_NEW[] =
    "INSERT INTO ifaces(vm_id, if_name, mac_addr, "
    "if_drv, vhost, macvtap, altname, netuser, bridge, bridge_eth) "
    "VALUES((SELECT id FROM vms WHERE name='%s'), "
    "'%s', '%s', '%s', %s, %s, '%s', %s, %s, '%s')";

static const char NM_SQL_IFACES_SELECT_MAX_MADDR[] =
    "SELECT COALESCE(MAX(mac_addr), 'de:ad:be:ef:00:00') FROM ifaces";

static const char NM_SQL_IFACES_SELECT_ID[] =
    "SELECT vm_id FROM ifaces WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') "
    "AND if_name='%s' AND if_drv='%s'";

static const char NM_SQL_IFACES_SELECT_MAC[] =
    "SELECT mac_addr FROM ifaces";

static const char NM_SQL_IFACES_SELECT_ID_BY_NAME[] =
    "SELECT vm_id FROM ifaces WHERE if_name='%s'";

static const char NM_SQL_IFACES_SELECT_JOIN_PARENT[] =
    "SELECT name, if_name FROM ifaces JOIN vms ON vms.id=ifaces.vm_id "
    "WHERE parent_eth='%s' OR parent_eth='%s'";

static const char NM_SQL_IFACES_SELECT_JOIN_TEAM[] =
    "SELECT name, if_name FROM ifaces JOIN vms ON vms.id=ifaces.vm_id "
    "AND vms.team='%s' WHERE parent_eth='%s' OR parent_eth='%s'";

static const char NM_SQL_IFACES_SELECT_BY_PARENT[] =
    "SELECT if_name FROM ifaces WHERE parent_eth='%s'";

static const char NM_SQL_IFACES_UPDATE_NAME[] =
    "UPDATE ifaces SET if_name='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_DRV[] =
    "UPDATE ifaces SET if_drv='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_MADDR[] =
    "UPDATE ifaces SET mac_addr='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_IPV4[] =
    "UPDATE ifaces SET ipv4_addr='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_PARENT[] =
    "UPDATE ifaces SET parent_eth='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_VHOST[] =
    "UPDATE ifaces SET vhost=%s WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_USERNET[] =
    "UPDATE ifaces SET netuser=%s WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_BRIDGE[] =
    "UPDATE ifaces SET bridge=%s WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_BETH[] =
    "UPDATE ifaces SET bridge_eth='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_HOSTFWD[] =
    "UPDATE ifaces SET hostfwd='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_SMB[] =
    "UPDATE ifaces SET smb='%s' WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_MACVTAP[] =
    "UPDATE ifaces SET macvtap=%zd WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_NAME_BY_ID[] =
    "UPDATE ifaces SET if_name='%s', altname='%s' "
    "WHERE vm_id=%s AND if_name='%s'";

static const char NM_SQL_IFACES_DELETE[] =
    "DELETE FROM ifaces WHERE vm_id=(SELECT id FROM vms WHERE name='%s') "
    "AND if_name='%s'";

static const char NM_SQL_IFACES_UPDATE_RESET[] =
    "UPDATE ifaces SET macvtap='0', parent_eth='' "
    "WHERE parent_eth='%s' OR parent_eth='%s'";

/* DRIVES */
static const char NM_SQL_DRIVES_INSERT_CLONED[] =
    "INSERT INTO drives(vm_id, drive_name, drive_drv, "
    "capacity, boot, discard, format) "
    "VALUES((SELECT id FROM vms WHERE name='%s'), "
    "'%s_%c.img', '%s', '%s', %s, %s, '%s')";

static const char NM_SQL_DRIVES_INSERT_NEW[] =
    "INSERT INTO drives(vm_id, drive_name, drive_drv, "
    "capacity, boot, discard, format) "
    "VALUES((SELECT id FROM vms WHERE name='%s'), "
    "'%s_a.img', '%s', '%s', %s, %s, '%s')";

static const char NM_SQL_DRIVES_INSERT_ADD[] =
    "INSERT INTO drives(vm_id, drive_name, drive_drv, "
    "capacity, boot, discard, format) "
    "VALUES((SELECT id FROM vms WHERE name='%s'), "
    "'%s_%c.img', '%s', '%s', 0, %s, '%s')";

static const char NM_SQL_DRIVES_INSERT_IMPORTED[] =
    "INSERT INTO drives(vm_id, drive_name, drive_drv, "
    "capacity, boot, discard, format) "
    "VALUES((SELECT id FROM vms WHERE name='%s'), "
    "'%s', '%s', '%s', '%s', '%s', '%s')";

static const char NM_SQL_DRIVES_SELECT[] =
    "SELECT drive_name, drive_drv, capacity, boot, discard, format "
    "FROM drives WHERE vm_id=(SELECT id FROM vms WHERE name='%s') "
    "ORDER BY drive_name ASC";

static const char NM_SQL_DRIVES_SELECT_BY_ID[] =
    "SELECT drive_name, drive_drv, capacity, boot, discard, format "
    "FROM drives WHERE vm_id=%s ORDER BY drive_name ASC";

static const char NM_SQL_DRIVES_SELECT_CAP[] =
    "SELECT drive_name, capacity FROM drives WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND boot='0'";

static const char NM_SQL_DRIVES_SELECT_BOOT[] =
    "SELECT drive_name FROM drives "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s') AND boot=1";

static const char NM_SQL_DRIVES_SELECT_NAME[] =
    "SELECT drive_name FROM drives WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s')";

static const char NM_SQL_DRIVES_CHECK_SNAP[] =
    "SELECT drive_name FROM drives "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s') AND boot=1 "
    "AND format='qcow2'";

static const char NM_SQL_DRIVES_UPDATE_DISCARD[] =
    "UPDATE drives SET discard=%s "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s')";

static const char NM_SQL_DRIVES_UPDATE_NAME_BY_ID[] =
    "UPDATE drives SET drive_name='%s' "
    "WHERE vm_id=%s AND drive_name='%s'";

static const char NM_SQL_DRIVES_UPDATE_DRV[] =
    "UPDATE drives SET drive_drv='%s' "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s')";

static const char NM_SQL_DRIVES_DELETE[] =
    "DELETE FROM drives WHERE vm_id=(SELECT id FROM vms WHERE name='%s') "
    "AND drive_name='%s'";

/* SNAPS */
static const char NM_SQL_SNAPS_UPDATE_RESET_LOAD[] =
    "UPDATE vmsnapshots SET load=0 WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s')";

static const char NM_SQL_SNAPS_SELECT_SNAP[] =
    "SELECT * FROM vmsnapshots WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND snap_name='%s'";

static const char NM_SQL_SNAPS_SELECT_ALL[] =
    "SELECT * FROM vmsnapshots WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') "
    "ORDER BY timestamp ASC";

static const char NM_SQL_SNAPS_UPDATE_SET_LOAD[] =
    "UPDATE vmsnapshots SET load=1 "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s') AND snap_name='%s'";

static const char NM_SQL_SNAPS_DELETE_SNAP[] =
    "DELETE FROM vmsnapshots WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND snap_name='%s'";

static const char NM_SQL_SNAPS_INSERT[] =
    "INSERT INTO vmsnapshots(snap_name, load, timestamp, vm_id) "
    "VALUES('%s', %d, DATETIME('now','localtime'), "
    "(SELECT id FROM vms WHERE name='%s'))";

static const char NM_SQL_SNAPS_UPDATE_PROPS[] =
    "UPDATE vmsnapshots SET load=%d, "
    "timestamp=DATETIME('now','localtime') "
    "WHERE vm_id=(SELECT id FROM vms WHERE name='%s') AND snap_name='%s'";

static const char NM_SQL_SNAPS_SELECT_LOAD[] =
    "SELECT snap_name FROM vmsnapshots WHERE "
    "vm_id=(SELECT id FROM vms WHERE name='%s') AND load=1";

/* USB */
static const char NM_SQL_USB_INSERT_NEW[] =
    "INSERT INTO usb(vm_id, dev_name, vendor_id, product_id, serial) "
    "VALUES ((SELECT id FROM vms WHERE name='%s'), '%s', '%s', '%s', '%s')";

static const char NM_SQL_USB_SELECT_ALL_BY_NAME[] =
    "SELECT * FROM usb WHERE vm_id=(SELECT id FROM vms WHERE name='%s')";

static const char NM_SQL_USB_SELECT_ALL_BY_ID[] =
    "SELECT * FROM usb WHERE vm_id=%s";

static const char NM_SQL_USB_SELECT_STATUS[] =
    "SELECT usb_status FROM vms WHERE name='%s'";

static const char NM_SQL_USB_SELECT_DEV[] =
    "SELECT vm_id FROM usb WHERE vm_id=(SELECT id FROM vms WHERE name='%s') "
    "AND dev_name='%s' AND vendor_id='%s' AND product_id='%s' AND serial='%s'";

static const char NM_SQL_USB_DELETE_ITEM[] =
    "DELETE FROM usb WHERE vm_id=(SELECT id FROM vms WHERE name='%s') "
    "AND dev_name='%s' "
    "AND vendor_id='%s' AND product_id='%s' AND serial='%s'";

/* VETH */
static const char NM_SQL_VETH_INSERT[] =
    "INSERT INTO veth(l_name, r_name) VALUES ('%s', '%s')";

static const char NM_SQL_VETH_SELECT[] =
    "SELECT l_name, r_name FROM veth";

static const char NM_SQL_VETH_SELECT_FORMATED[] =
    "SELECT (l_name || '<->' || r_name) FROM veth ORDER by l_name ASC";

static const char NM_SQL_VETH_SELECT_NAME[] =
    "SELECT l_name FROM veth WHERE l_name='%s' OR r_name='%s'";

static const char NM_SQL_VETH_DELETE[] =
    "DELETE FROM veth WHERE l_name='%s'";

typedef sqlite3 nm_sqlite_t;

typedef struct {
    nm_sqlite_t *handler;
    bool in_transaction;
} db_conn_t;

#define NM_INIT_DB_CONN (db_conn_t) {NULL, false}

void nm_db_init(void);
void nm_db_select(const char *query, nm_vect_t *v);
void nm_db_select_value(const char *query, nm_str_t *res);
void nm_db_edit(const char *query);
bool nm_db_in_transaction(void);
void nm_db_begin_transaction(void);
void nm_db_atomic(const char *query);
void nm_db_commit(void);
void nm_db_rollback(void);
void nm_db_close(void);

enum select_main_idx {
    NM_SQL_ID = 0,
    NM_SQL_NAME,
    NM_SQL_MEM,
    NM_SQL_SMP,
    NM_SQL_KVM,
    NM_SQL_HCPU,
    NM_SQL_VNC,
    NM_SQL_ARCH,
    NM_SQL_ISO,
    NM_SQL_INST,
    NM_SQL_USBF,
    NM_SQL_USBD,
    NM_SQL_BIOS,
    NM_SQL_KERN,
    NM_SQL_OVER,
    NM_SQL_KAPP,
    NM_SQL_TTY,
    NM_SQL_SOCK,
    NM_SQL_INIT,
    NM_SQL_MACH,
    NM_SQL_9FLG,
    NM_SQL_9PTH,
    NM_SQL_9ID,
    NM_SQL_USBT,
    NM_SQL_SPICE,
    NM_SQL_DEBP,
    NM_SQL_DEBF,
    NM_SQL_ARGS,
    NM_SQL_GROUP,
    NM_SQL_DISPLAY,
    NM_SQL_FLASH,
    NM_SQL_AGENT,
    NM_VM_IDX_COUNT
};

enum select_ifs_idx {
    NM_SQL_IF_NAME = 0,
    NM_SQL_IF_MAC,
    NM_SQL_IF_DRV,
    NM_SQL_IF_IP4,
    NM_SQL_IF_VHO,
    NM_SQL_IF_MVT,
    NM_SQL_IF_PET,
    NM_SQL_IF_ALT,
    NM_SQL_IF_USR,
    NM_SQL_IF_BGE,
    NM_SQL_IF_BETH,
    NM_SQL_IF_FWD,
    NM_SQL_IF_SMB,
    NM_IFS_IDX_COUNT
};

enum select_drive_idx {
    NM_SQL_DRV_NAME = 0,
    NM_SQL_DRV_TYPE,
    NM_SQL_DRV_SIZE,
    NM_SQL_DRV_BOOT,
    NM_SQL_DRV_DISC,
    NM_SQL_DRV_FMT,
    NM_DRV_IDX_COUNT
};

enum select_usb_idx {
    NM_SQL_USB_NAME = 0,
    NM_SQL_USB_VID,
    NM_SQL_USB_PID,
    NM_SQL_USB_SERIAL,
    NM_USB_IDX_COUNT
};

enum {
    NM_SQL_VMSNAP_NAME = 0,
    NM_SQL_VMSNAP_LOAD,
    NM_SQL_VMSNAP_TIME,
    NM_SQL_VMSNAP_VMID,
    NM_SQL_VMSNAP_COUNT
};


#endif /* NM_DATABASE_H_ */
/* vim:set ts=4 sw=4: */
