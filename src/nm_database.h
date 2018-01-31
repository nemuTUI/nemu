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

#define NM_USB_EXISTS_SQL \
    "SELECT id FROM usb WHERE vm_name='%s' AND dev_name='%s' " \
    "AND vendor_id='%s' AND product_id='%s' AND serial='%s'"

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
    NM_SQL_9ID
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
