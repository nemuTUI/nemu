#ifndef NM_USB_DEVICES_H_
#define NM_USB_DEVICES_H_

#include <nm_string.h>
#include <nm_vector.h>

typedef struct {
    nm_str_t name;
    nm_str_t id;
} nm_usb_dev_t;

void nm_usb_get_devs(nm_vect_t *v);
void nm_usb_vect_ins_cb(const void *unit_p, const void *ctx);
void nm_usb_vect_free_cb(const void *unit_p);
void nm_usb_parse_dev(const nm_str_t *src, nm_str_t *b_num, nm_str_t *d_addr);

#define NM_INIT_USB { NM_INIT_STR, NM_INIT_STR }

#define nm_usb_name(p) ((nm_usb_dev_t *) p)->name
#define nm_usb_id(p) ((nm_usb_dev_t *) p)->id

#endif /* NM_USB_DEVICES_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
