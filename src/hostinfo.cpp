#include <libudev.h>
#include <libusb.h>

#include "hostinfo.h"

static const char *hwdb_get(const char *modalias, const char *key);
static const char *get_vendor(uint16_t vid);
static const char *get_product(uint16_t vid, uint16_t pid);
static int get_vendor_string(char *buf, size_t size, uint16_t vid);
static int get_product_string(char *buf, size_t size, uint16_t vid, uint16_t pid);

static QManager::MapString u_list;
static struct udev_hwdb *hwdb = NULL;

static const char *hwdb_get(const char *modalias, const char *key)
{
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias, 0))
    {
        if (strcmp(udev_list_entry_get_name(entry), key) == 0)
            return udev_list_entry_get_value(entry);
    }

    return NULL;
}

static const char *get_vendor(uint16_t vid)
{
    char modalias[64];

    sprintf(modalias, "usb:v%04X*", vid);
    return hwdb_get(modalias, "ID_VENDOR_FROM_DATABASE");
}

static const char *get_product(uint16_t vid, uint16_t pid)
{
    char modalias[64];

    sprintf(modalias, "usb:v%04Xp%04X*", vid, pid);
    return hwdb_get(modalias, "ID_MODEL_FROM_DATABASE");
}

static int get_vendor_string(char *buf, size_t size, uint16_t vid)
{
    const char *cp;

    if (size < 1)
        return 0;

    *buf = 0;

    if (!(cp = get_vendor(vid)))
        return 0;

    return snprintf(buf, size, "%s", cp);
}

static int get_product_string(char *buf, size_t size, uint16_t vid, uint16_t pid)
{
    const char *cp;

    if (size < 1)
        return 0;

    *buf = 0;

    if (!(cp = get_product(vid, pid)))
        return 0;

    return snprintf(buf, size, "%s", cp);
}

uint32_t QManager::total_memory()
{
    meminfo();
    return kb_main_total / 1024;
}

uint32_t QManager::disk_free(const std::string &vmdir)
{
    struct statvfs df;
    statvfs(vmdir.c_str(), &df);

    return (df.f_bsize * df.f_bavail) / 1024 / 1024 / 1024;
}

uint32_t QManager::cpu_count()
{
    uint32_t cores = std::thread::hardware_concurrency();

    return cores;
}

QManager::MapString *QManager::get_usb_list()
{
    std::string usb_id, usb_name;
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    struct udev *udev = NULL;
    int rc;
    ssize_t dev_count, n;
    char vendor[128], product[128];

    try
    {
        if ((udev = udev_new()) == NULL)
            throw std::runtime_error("udev_new failed");

        if ((hwdb = udev_hwdb_new(udev)) == NULL)
            throw std::runtime_error("udev_hwdb_new failed");

        if ((rc = libusb_init(&ctx)) != 0)
            throw std::runtime_error(libusb_strerror(rc));

        if ((dev_count = libusb_get_device_list(ctx, &list)) < 1)
            throw std::runtime_error("libusb_get_device_list failed");

        for (n = 0; n < dev_count; n++)
        {
            libusb_device *device = list[n];
            struct libusb_device_descriptor desc = {};

            if (libusb_get_device_descriptor(device, &desc) != 0)
                continue;

            if (get_vendor_string(vendor, sizeof(vendor), desc.idVendor) == 0)
                sprintf(vendor, "vendor-unknown");
            if (get_product_string(product, sizeof(product), desc.idVendor, desc.idProduct) == 0)
                sprintf(product, "product-unknown");

            printf("%04x:%04x %s %s\n", desc.idVendor, desc.idProduct, vendor, product);
        }

        u_list.clear();

        u_list.insert(std::make_pair(usb_name, usb_id));
    }
    catch (const std::runtime_error &err)
    {
        if (list)
            libusb_free_device_list(list, 1);
        if (hwdb)
            udev_hwdb_unref(hwdb);
        if (udev)
            udev_unref(udev);
        /* TODO: log error */
        return NULL;
    }

    libusb_free_device_list(list, 1);
    udev_hwdb_unref(hwdb);
    udev_unref(udev);

    return &u_list;
}
