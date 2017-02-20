#include <libudev.h>
#include <libusb.h>

#include "hostinfo.h"

static QManager::MapString u_list;

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
    u_list.clear();
    std::string usb_id, usb_name;

    u_list.insert(std::make_pair(usb_name, usb_id));

    //return &u_list;
    return NULL;
}
