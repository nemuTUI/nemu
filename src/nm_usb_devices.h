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
void nm_usb_vect_ins_cb(void *unit_p, const void *ctx);
void nm_usb_vect_free_cb(void *unit_p);
void nm_usb_data_vect_ins_cb(void *unit_p, const void *ctx);
void nm_usb_data_vect_free_cb(void *unit_p);
int nm_usb_get_serial(const nm_usb_dev_t *dev, nm_str_t *serial);
void nm_usb_data_free(nm_usb_data_t *dev);

#define NM_INIT_USB { NM_INIT_STR, NM_INIT_STR, NM_INIT_STR, 0, 0 }
#define NM_INIT_USB_DATA { NM_INIT_STR, NULL }

static inline nm_str_t *nm_usb_name(const nm_usb_dev_t *p)
{
    return (nm_str_t *)&p->name;
}
static inline nm_str_t *nm_usb_vendor_id(const nm_usb_dev_t *p)
{
    return (nm_str_t *)&p->vendor_id;
}
static inline nm_str_t *nm_usb_product_id(const nm_usb_dev_t *p)
{
    return (nm_str_t *)&p->product_id;
}
static inline uint8_t *nm_usb_bus_num(const nm_usb_dev_t *p)
{
    return (uint8_t *)&p->bus_num;
}
static inline uint8_t *nm_usb_dev_addr(const nm_usb_dev_t *p)
{
    return (uint8_t *)&p->dev_addr;
}

static inline nm_str_t *nm_usb_data_serial(const nm_usb_data_t *p)
{
    return (nm_str_t *)&p->serial;
}
static inline nm_usb_dev_t **nm_usb_data_dev(const nm_usb_data_t *p)
{
    return (nm_usb_dev_t **)&p->dev;
}

#endif /* NM_USB_DEVICES_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
