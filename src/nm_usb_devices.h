#ifndef NM_USB_DEVICES_H_
#define NM_USB_DEVICES_H_

#include <nm_string.h>
#include <nm_vector.h>

typedef struct {
    nm_str_t name;
    nm_str_t vendor_id;
    nm_str_t product_id;
    uint8_t bus_num;
    uint8_t dev_addr;
} nm_usb_dev_t;

typedef struct {
    nm_str_t serial;
    nm_usb_dev_t *dev;
} nm_usb_data_t;

void nm_usb_get_devs(nm_vect_t *v);
void nm_usb_vect_ins_cb(const void *unit_p, const void *ctx);
void nm_usb_vect_free_cb(const void *unit_p);
void nm_usb_data_vect_ins_cb(const void *unit_p, const void *ctx);
void nm_usb_data_vect_free_cb(const void *unit_p);
void nm_usb_parse_dev(const nm_str_t *src, nm_str_t *b_num, nm_str_t *d_addr);
int nm_usb_get_serial(const nm_usb_dev_t *dev, nm_str_t *serial);
void nm_usb_data_free(nm_usb_data_t *dev);

#define NM_INIT_USB { NM_INIT_STR, NM_INIT_STR, NM_INIT_STR, 0, 0 }
#define NM_INIT_USB_DATA { NM_INIT_STR, NULL }

#define nm_usb_name(p) ((nm_usb_dev_t *) p)->name
#define nm_usb_vendor_id(p) ((nm_usb_dev_t *) p)->vendor_id
#define nm_usb_product_id(p) ((nm_usb_dev_t *) p)->product_id
#define nm_usb_bus_num(p) ((nm_usb_dev_t *) p)->bus_num
#define nm_usb_dev_addr(p) ((nm_usb_dev_t *) p)->dev_addr
#define nm_usb_data_serial(p) ((nm_usb_data_t *) p)->serial
#define nm_usb_data_dev(p) ((nm_usb_data_t *) p)->dev

#endif /* NM_USB_DEVICES_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
