#include <nm_core.h>
#include <nm_utils.h>
#include <nm_usb_devices.h>

/* Disable USB on FreeBSD while libudev-devd
 * will not supports calls udev_hwdb_* */

enum {NM_USB_SERIAL_LEN = 127};

static inline void nm_usb_dev_free(nm_usb_dev_t *dev);

#if defined (NM_OS_LINUX)
#include <libudev.h>
#include <libusb.h>

static const char *nm_usb_hwdb_get(const char *modalias, const char *key);
static const char *nm_usb_get_vendor(uint16_t vid);
static const char *nm_usb_get_product(uint16_t vid, uint16_t pid);
static int nm_usb_get_vendor_str(char *buf, size_t size, uint16_t vid);
static int nm_usb_get_product_str(char *buf, size_t size, uint16_t vid, uint16_t pid);

static struct udev_hwdb *hwdb = NULL;
#endif /* NM_OS_LINUX */

void nm_usb_get_devs(nm_vect_t *v)
{
#if defined (NM_OS_LINUX)
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    struct udev *udev = NULL;
    int rc;
    ssize_t dev_count, n;
    char vendor[128], product[128];

    if ((udev = udev_new()) == NULL)
        nm_bug(_("%s: udev_new failed"), __func__);

    if ((hwdb = udev_hwdb_new(udev)) == NULL)
        nm_bug(_("%s: udev_hwdb_new failed"), __func__);

    if ((rc = libusb_init(&ctx)) != 0)
        nm_bug("%s: %s", __func__, libusb_strerror(rc));

    if ((dev_count = libusb_get_device_list(ctx, &list)) < 1)
        nm_bug(_("%s: libusb_get_device_list failed"), __func__);

    //@TODO Some of variables may be moved outside, and freed only once
    for (n = 0; n < dev_count; n++)
    {
        nm_usb_dev_t dev = NM_INIT_USB;
        libusb_device *device = list[n];
        struct libusb_device_descriptor desc;

        memset(&desc, 0, sizeof(desc));

        if (libusb_get_device_descriptor(device, &desc) != 0)
            continue;

        if (nm_usb_get_vendor_str(vendor, sizeof(vendor), desc.idVendor) == 0)
            nm_str_alloc_text(&dev.name, "vendor-unknown");
        else
            nm_str_alloc_text(&dev.name, vendor);

        if (nm_usb_get_product_str(product, sizeof(product), desc.idVendor, desc.idProduct) == 0)
            nm_str_add_text(&dev.name, " product-unknown");
        else
            nm_str_add_text(&dev.name, product);

        dev.bus_num = libusb_get_bus_number(device);
        dev.dev_addr = libusb_get_device_address(device);

        nm_str_format(&dev.vendor_id, "%04x", desc.idVendor);
        nm_str_format(&dev.product_id, "%04x", desc.idProduct);

        nm_vect_insert(v, &dev, sizeof(dev), nm_usb_vect_ins_cb);
        nm_usb_dev_free(&dev);
    }

    /* cleanup */
    libusb_free_device_list(list, 1);
    udev_hwdb_unref(hwdb);
    udev_unref(udev);
    libusb_exit(ctx);
#else
    (void) v;
#endif /* NM_OS_LINUX */
}

int nm_usb_get_serial(const nm_usb_dev_t *dev, nm_str_t *serial)
{
    int rc = NM_ERR;
#if defined (NM_OS_LINUX)
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    int usb_rc;
    ssize_t dev_count;

    if (dev == NULL)
        nm_bug(_("%s: null nm_usb_dev_t pointer"), __func__);

    if ((usb_rc = libusb_init(&ctx)) != 0)
        nm_bug("%s: %s", __func__, libusb_strerror(usb_rc));

    if ((dev_count = libusb_get_device_list(ctx, &list)) < 1)
        nm_bug(_("%s: libusb_get_device_list failed"), __func__);

    for (ssize_t n = 0; n < dev_count; n++)
    {
        uint8_t bus_num, dev_addr;
        libusb_device *device = list[n];

        bus_num = libusb_get_bus_number(device);
        dev_addr = libusb_get_device_address(device);

        if ((bus_num == dev->bus_num) &&
            (dev_addr == dev->dev_addr))
        {
            libusb_device_handle *handle;
            char serial_buf[NM_USB_SERIAL_LEN] = {0};
            struct libusb_device_descriptor desc;

            memset(&desc, 0, sizeof(desc));

            if (libusb_get_device_descriptor(device, &desc) != 0)
                break;

            if ((usb_rc = libusb_open(device, &handle)) != 0)
                nm_bug("%s: %s", __func__, libusb_strerror(usb_rc));

            if (libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                        (uint8_t *) serial_buf, NM_USB_SERIAL_LEN) > 0)
            {
                nm_str_alloc_text(serial, serial_buf);
                rc = NM_OK;
            }

            libusb_close(handle);

            break;
        }
    }

    /* cleanup */
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
#else
    (void) dev;
    (void) serial;
#endif /* NM_OS_LINUX */

    return rc;
}

void nm_usb_vect_ins_cb(void *unit_p, const void *ctx)
{
    nm_str_copy(nm_usb_name(unit_p), nm_usb_name(ctx));
    nm_str_copy(nm_usb_vendor_id(unit_p), nm_usb_vendor_id(ctx));
    nm_str_copy(nm_usb_product_id(unit_p), nm_usb_product_id(ctx));
    *nm_usb_bus_num(unit_p) = *nm_usb_bus_num(ctx);
    *nm_usb_dev_addr(unit_p) = *nm_usb_dev_addr(ctx);
}

void nm_usb_vect_free_cb(void *unit_p)
{
    nm_str_free(nm_usb_name(unit_p));
    nm_str_free(nm_usb_vendor_id(unit_p));
    nm_str_free(nm_usb_product_id(unit_p));
}

void nm_usb_data_vect_ins_cb(void *unit_p, const void *ctx)
{
    nm_str_copy(nm_usb_data_serial(unit_p), nm_usb_data_serial(ctx));
    *nm_usb_data_dev(unit_p) = *nm_usb_data_dev(ctx);
}

void nm_usb_data_vect_free_cb(void *unit_p)
{
    nm_str_free(nm_usb_data_serial(unit_p));
}

void nm_usb_data_free(nm_usb_data_t *usb)
{
    nm_str_free(&usb->serial);
    nm_usb_dev_free(usb->dev);
}

static inline void nm_usb_dev_free(nm_usb_dev_t *dev)
{
    nm_str_free(&dev->name);
    nm_str_free(&dev->vendor_id);
    nm_str_free(&dev->product_id);
}

#if defined (NM_OS_LINUX)
static const char *nm_usb_hwdb_get(const char *modalias, const char *key)
{
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias, 0))
    {
        if (strcmp(udev_list_entry_get_name(entry), key) == 0)
            return udev_list_entry_get_value(entry);
    }

    return NULL;
}

static const char *nm_usb_get_vendor(uint16_t vid)
{
    char modalias[64];

    sprintf(modalias, "usb:v%04X*", vid);
    return nm_usb_hwdb_get(modalias, "ID_VENDOR_FROM_DATABASE");
}

static const char *nm_usb_get_product(uint16_t vid, uint16_t pid)
{
    char modalias[64];

    sprintf(modalias, "usb:v%04Xp%04X*", vid, pid);
    return nm_usb_hwdb_get(modalias, "ID_MODEL_FROM_DATABASE");
}

static int nm_usb_get_vendor_str(char *buf, size_t size, uint16_t vid)
{
    const char *cp;

    if (size < 1)
        return 0;

    *buf = 0;

    if (!(cp = nm_usb_get_vendor(vid)))
        return 0;

    return snprintf(buf, size, "%s ", cp);
}

static int nm_usb_get_product_str(char *buf, size_t size, uint16_t vid, uint16_t pid)
{
    const char *cp;

    if (size < 1)
        return 0;

    *buf = 0;

    if (!(cp = nm_usb_get_product(vid, pid)))
        return 0;

    return snprintf(buf, size, "%s", cp);
}
#endif /* NM_OS_LINUX */

/* vim:set ts=4 sw=4 fdm=marker: */
