#ifndef NM_DATABASE_H_
#define NM_DATABASE_H_

#include <nm_vector.h>
#include <stdbool.h>

#define NM_DB_VERSION "14"

//@TODO Those queries should have constant naming convention and some kind of sorting
static const char NM_GET_VMS_SQL[] = \
    "SELECT name FROM vms ORDER BY name ASC";

static const char NM_GET_VMS_FILTER_GROUP_SQL[] = \
    "SELECT name FROM vms WHERE team='%s' ORDER BY name ASC";

static const char NM_CLONE_VMS_SQL[] = \
    "INSERT INTO vms SELECT NULL, '%s', mem, smp, kvm, hcpu, '%d', arch, iso, " \
    "install, usb, usbid, bios, kernel, mouse_override, kernel_append, tty_path, " \
    "socket_path, initrd, machine, fs9p_enable, fs9p_path, fs9p_name, usb_type, " \
    "spice, debug_port, debug_freeze, cmdappend, team, display_type FROM vms WHERE name='%s'";

static const char NM_RESET_LOAD_SQL[] = \
    "UPDATE vmsnapshots SET load='0' WHERE vm_name='%s'";

static const char NM_USB_GET_SQL[] = \
    "SELECT * FROM usb WHERE vm_name='%s'";

static const char NM_USB_ADD_SQL[] = \
    "INSERT INTO usb(vm_name, dev_name, vendor_id, product_id, serial) " \
    "VALUES ('%s', '%s', '%s', '%s', '%s')";

static const char NM_USB_DELETE_SQL[] = \
    "DELETE FROM usb WHERE vm_name='%s' AND dev_name='%s' " \
    "AND vendor_id='%s' AND product_id='%s' AND serial='%s'";

static const char NM_USB_CHECK_SQL[] = \
    "SELECT usbid FROM vms WHERE name='%s'";

static const char NM_DEL_DRIVES_SQL[] = \
    "DELETE FROM drives WHERE vm_name='%s'";

static const char NM_DEL_DRIVE_SQL[] = \
    "DELETE FROM drives WHERE vm_name='%s' AND drive_name='%s'";

static const char NM_DEL_VMSNAP_SQL[] = \
    "DELETE FROM vmsnapshots WHERE vm_name='%s'";

static const char NM_DEL_IFS_SQL[] = \
    "DELETE FROM ifaces WHERE vm_name='%s'";

static const char NM_DEL_USB_SQL[] = \
    "DELETE FROM usb WHERE vm_name='%s'";

static const char NM_DEL_VM_SQL[] = \
    "DELETE FROM vms WHERE name='%s'";

static const char NM_USB_EXISTS_SQL[] = \
    "SELECT id FROM usb WHERE vm_name='%s' AND dev_name='%s' " \
    "AND vendor_id='%s' AND product_id='%s' AND serial='%s'";

static const char NM_VM_GET_LIST_SQL[] = \
    "SELECT * FROM vms WHERE name='%s'";

static const char NM_VM_GET_IFACES_SQL [] = \
    "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost, " \
    "macvtap, parent_eth, altname FROM ifaces " \
    "WHERE vm_name='%s' ORDER BY if_name ASC";

static const char NM_VM_GET_DRIVES_SQL[] = \
    "SELECT drive_name, drive_drv, capacity, boot " \
    "FROM drives WHERE vm_name='%s' ORDER BY id ASC";

static const char NM_VM_GET_ADDDRIVES_SQL[] = \
    "SELECT drive_name, capacity FROM drives WHERE vm_name='%s' " \
    "AND boot='0'";

static const char NM_SNAP_GET_NAME_SQL[] = \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s' " \
    "AND snap_name='%s'";

static const char NM_GET_SNAPS_ALL_SQL[] = \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s' " \
    "ORDER BY timestamp ASC";

static const char NM_GET_SNAPS_NAME_SQL[] = \
    "SELECT snap_name FROM vmsnapshots WHERE vm_name='%s' " \
    "ORDER BY timestamp ASC";

static const char NM_SNAP_UPDATE_LOAD_SQL[] = \
    "UPDATE vmsnapshots SET load='1' " \
    "WHERE vm_name='%s' AND snap_name='%s'";

static const char NM_DELETE_SNAP_SQL[] = \
    "DELETE FROM vmsnapshots WHERE vm_name='%s' " \
    "AND snap_name='%s'";

static const char NM_INSERT_SNAP_SQL[] = \
    "INSERT INTO vmsnapshots(vm_name, snap_name, load, timestamp) " \
    "VALUES('%s', '%s', '%d', DATETIME('now','localtime'))";

static const char NM_UPDATE_SNAP_SQL[] = \
    "UPDATE vmsnapshots SET load='%d', " \
    "timestamp=DATETIME('now','localtime') " \
    "WHERE vm_name='%s' AND snap_name='%s'";

static const char NM_CHECK_SNAP_SQL[] = \
    "SELECT id FROM snapshots WHERE vm_name='%s'";

static const char NM_GET_BOOT_DRIVE_SQL[] = \
    "SELECT drive_name FROM drives " \
    "WHERE vm_name='%s' AND boot='1'";

static const char NM_SELECT_DRIVE_NAMES_SQL[] = \
    "SELECT drive_name FROM drives WHERE vm_name='%s'";

static const char NM_GET_VETH_SQL[] = \
    "SELECT l_name, r_name FROM veth";

static const char NM_LAN_GET_VETH_SQL[] = \
    "SELECT (l_name || '<->' || r_name) FROM veth ORDER by l_name ASC";

static const char NM_LAN_ADD_VETH_SQL[] = \
    "INSERT INTO veth(l_name, r_name) VALUES ('%s', '%s')";

static const char NM_LAN_CHECK_NAME_SQL[] = \
    "SELECT id FROM veth WHERE l_name='%s' OR r_name='%s'";

static const char NM_LAN_DEL_VETH_SQL[] = \
    "DELETE FROM veth WHERE l_name='%s'";

static const char NM_GET_IFACES_SQL[] = \
    "SELECT if_name FROM ifaces WHERE vm_name='%s'";

static const char NM_GET_IFACE_SQL[] = \
    "SELECT id FROM ifaces WHERE vm_name='%s' AND if_name='%s' AND if_drv='%s'";

static const char NM_DEL_IFACE_SQL[] = \
    "DELETE FROM ifaces WHERE vm_name='%s' AND if_name='%s'";

static const char NM_GET_IFACES_MACS[] = \
    "SELECT mac_addr FROM ifaces";

static const char NM_GET_IFMAP_SQL[] = \
    "SELECT vm_name, if_name FROM ifaces WHERE parent_eth='%s' " \
    "OR parent_eth='%s'";

static const char NM_GET_IFMAPGR_SQL[] = \
    "SELECT vm_name, if_name FROM ifaces JOIN vms ON " \
    "vm_name=name WHERE team='%s' AND (parent_eth='%s' OR parent_eth='%s')";

static const char NM_GET_GROUPS_SQL[] = \
    "SELECT DISTINCT team FROM vms WHERE team IS NOT NULL";

static const char NM_LAN_VETH_INF_SQL[] = \
    "SELECT if_name FROM ifaces WHERE parent_eth='%s'";

static const char NM_LAN_VETH_DEP_SQL[] = \
    "UPDATE ifaces SET macvtap='0', parent_eth='' " \
    "WHERE parent_eth='%s' OR parent_eth='%s'";

static const char NM_GET_VMSNAP_LOAD_SQL[] = \
    "SELECT snap_name FROM vmsnapshots WHERE vm_name='%s' " \
    "AND load='1'";

static const char NM_USB_UPDATE_STATE_SQL[] = \
    "UPDATE vms SET usbid='%s' WHERE name='%s'";

static const char NM_VMCTL_GET_VNC_PORT_SQL[] = \
    "SELECT vnc, spice FROM vms WHERE name='%s'";

static const char NM_GET_DB_VERSION_SQL[] = \
    "PRAGMA user_version";

void nm_db_init(void);
void nm_db_select(const char *query, nm_vect_t *v);
void nm_db_edit(const char *query);
bool nm_db_in_transaction();
void nm_db_begin_transaction();
void nm_db_atomic(const char *query);
void nm_db_commit();
void nm_db_rollback();
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
    NM_IFS_IDX_COUNT
};

enum select_drive_idx {
    NM_SQL_DRV_NAME = 0,
    NM_SQL_DRV_TYPE,
    NM_SQL_DRV_SIZE,
    NM_SQL_DRV_BOOT,
    NM_DRV_IDX_COUNT
};

enum select_usb_idx {
    NM_SQL_USB_ID = 0,
    NM_SQL_USB_VMNAME,
    NM_SQL_USB_NAME,
    NM_SQL_USB_VID,
    NM_SQL_USB_PID,
    NM_SQL_USB_SERIAL,
    NM_USB_IDX_COUNT
};

#endif /* NM_DATABASE_H_ */
/* vim:set ts=4 sw=4: */
