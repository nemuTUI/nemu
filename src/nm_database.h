#ifndef NM_DATABASE_H_
#define NM_DATABASE_H_

#include <nm_vector.h>

#define NM_RESET_LOAD_SQL \
    "UPDATE vmsnapshots SET load='0' WHERE vm_name='%s'"

#define NM_USB_GET_SQL \
    "SELECT * FROM usb WHERE vm_name='%s'"

#define NM_USB_ADD_SQL \
    "INSERT INTO usb(vm_name, dev_name, vendor_id, product_id, serial) " \
    "VALUES ('%s', '%s', '%s', '%s', '%s')"

#define NM_USB_DELETE_SQL \
    "DELETE FROM usb WHERE vm_name='%s' AND dev_name='%s' " \
    "AND vendor_id='%s' AND product_id='%s' AND serial='%s'"

#define NM_USB_CHECK_SQL \
    "SELECT usbid FROM vms WHERE name='%s'"

#define NM_DEL_DRIVES_SQL \
    "DELETE FROM drives WHERE vm_name='%s'"

#define NM_DEL_VMSNAP_SQL \
    "DELETE FROM vmsnapshots WHERE vm_name='%s'"

#define NM_DEL_IFS_SQL \
    "DELETE FROM ifaces WHERE vm_name='%s'"

#define NM_DEL_USB_SQL \
    "DELETE FROM usb WHERE vm_name='%s'"

#define NM_DEL_VM_SQL \
    "DELETE FROM vms WHERE name='%s'"

#define NM_USB_EXISTS_SQL \
    "SELECT id FROM usb WHERE vm_name='%s' AND dev_name='%s' " \
    "AND vendor_id='%s' AND product_id='%s' AND serial='%s'"

#define NM_VM_GET_LIST_SQL \
    "SELECT * FROM vms WHERE name='%s'"

#define NM_VM_GET_IFACES_SQL  \
    "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost, " \
    "macvtap, parent_eth FROM ifaces WHERE vm_name='%s' ORDER BY if_name ASC"

#define NM_VM_GET_DRIVES_SQL \
    "SELECT drive_name, drive_drv, capacity, boot " \
    "FROM drives WHERE vm_name='%s' ORDER BY id ASC"

#define NM_VM_GET_ADDDRIVES_SQL \
    "SELECT drive_name, capacity FROM drives WHERE vm_name='%s' " \
    "AND boot='0'"

#define NM_SNAP_GET_NAME_SQL \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s' " \
    "AND snap_name='%s'"

#define NM_GET_SNAPS_ALL_SQL \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s' " \
    "ORDER BY timestamp ASC"

#define NM_GET_SNAPS_NAME_SQL \
    "SELECT snap_name FROM vmsnapshots WHERE vm_name='%s' " \
    "ORDER BY timestamp ASC"

#define NM_SNAP_UPDATE_LOAD_SQL \
    "UPDATE vmsnapshots SET load='1' " \
    "WHERE vm_name='%s' AND snap_name='%s'"

#define NM_DELETE_SNAP_SQL \
    "DELETE FROM vmsnapshots WHERE vm_name='%s' " \
    "AND snap_name='%s'"

#define NM_UPDATE_SNAP_SQL \
    "UPDATE vmsnapshots SET load='%d', " \
    "timestamp=DATETIME('now','localtime') " \
    "WHERE vm_name='%s' AND snap_name='%s'"

#define NM_CHECK_SNAP_SQL \
    "SELECT id FROM snapshots WHERE vm_name='%s'"

#define NM_GET_BOOT_DRIVE_SQL \
    "SELECT drive_name FROM drives " \
    "WHERE vm_name='%s' AND boot='1'"

#define NM_SELECT_DRIVE_NAMES_SQL \
    "SELECT drive_name FROM drives WHERE vm_name='%s'"

#define NM_GET_IFACES_SQL \
    "SELECT if_name FROM ifaces WHERE vm_name='%s'"

#define NM_GET_VETH_SQL \
    "SELECT l_name, r_name FROM veth"

#define NM_GET_IFMAP_SQL \
    "SELECT vm_name, if_name FROM ifaces WHERE parent_eth='%s'" \
    "OR parent_eth='%s'"

void nm_db_init(void);
void nm_db_select(const char *query, nm_vect_t *v);
void nm_db_edit(const char *query);
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
    NM_SQL_DEBS
};

enum select_ifs_idx {
    NM_SQL_IF_NAME = 0,
    NM_SQL_IF_MAC,
    NM_SQL_IF_DRV,
    NM_SQL_IF_IP4,
    NM_SQL_IF_VHO,
    NM_SQL_IF_MVT,
    NM_SQL_IF_PET
};

enum select_drive_idx {
    NM_SQL_DRV_NAME = 0,
    NM_SQL_DRV_TYPE,
    NM_SQL_DRV_SIZE,
    NM_SQL_DRV_BOOT
};

enum select_usb_idx {
    NM_SQL_USB_ID = 0,
    NM_SQL_USB_VMNAME,
    NM_SQL_USB_NAME,
    NM_SQL_USB_VID,
    NM_SQL_USB_PID,
    NM_SQL_USB_SERIAL
};

#define NM_IFS_IDX_COUNT 7
#define NM_DRV_IDX_COUNT 4
#define NM_USB_IDX_COUNT 6

#endif /* NM_DATABASE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
