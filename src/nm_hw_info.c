#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_cfg_file.h>
#include <nm_hw_info.h>

#include <sys/statvfs.h>

#if defined(NM_OS_LINUX)
#include <sys/sysinfo.h>
#elif defined(NM_OS_FREEBSD)
#include <sys/sysctl.h>
#endif

uint32_t nm_hw_total_ram(void)
{
    uint32_t ram = 0;
#if defined(NM_OS_LINUX)
    struct sysinfo info;

    memset(&info, 0, sizeof(info));
    sysinfo(&info);
    ram = info.totalram / 1024 / 1024;
#elif defined(NM_OS_FREEBSD)
    uint64_t mem;
    int mib[2];
    size_t len = sizeof(mem);

    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;

    if (sysctl(mib, 2, &mem, &len, NULL, 0) != 0) {
        nm_bug("%s: sysctl: %s", __func__, strerror(errno));
    }

    ram = mem / 1024 / 1024;
#endif

    return ram;
}

uint32_t nm_hw_disk_free(void)
{
    uint64_t df = 0;
    struct statvfs fsstat;

    memset(&fsstat, 0, sizeof(fsstat));

    if (statvfs(nm_cfg_get()->vm_dir.data, &fsstat) != 0) {
        nm_bug("%s: statvfs: %s", __func__, strerror(errno));
    }
    df = (fsstat.f_frsize * fsstat.f_bavail) / 1024 / 1024 / 1024;

    /* reserve 1 Gb */
    df--;

    return df;
}

/* vim:set ts=4 sw=4: */
