#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_cfg_file.h>

#include <sys/statvfs.h>

#if defined (NM_OS_LINUX)
#include <sys/sysinfo.h>
#elif defined (NM_OS_FREEBSD)
#include <sys/sysctl.h>
#endif

uint32_t nm_hw_total_ram(void)
{
    uint32_t ram = 0;
#if defined (NM_OS_LINUX)
    struct sysinfo info;

    memset(&info, 0, sizeof(info));
    sysinfo(&info);
    ram = info.totalram / 1024 / 1024;
#elif defined (NM_OS_FREEBSD)
    uint64_t mem;
    int mib[2];
    size_t len = sizeof(mem);

    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;

    if (sysctl(mib, 2, &mem, &len, NULL, 0) != 0)
        nm_bug("%s: sysctl: %s", __func__, strerror(errno));

    ram = mem / 1024 / 1024;
#endif

    return ram;
}

uint32_t nm_hw_ncpus(void)
{
    uint32_t ncpus = 0;
#if defined (NM_OS_LINUX)
    ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined (NM_OS_FREEBSD)
    int mib[2];
    size_t len = sizeof(ncpus);

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    if (sysctl(mib, 2, &ncpus, &len, NULL, 0) != 0)
        nm_bug("%s: sysctl: %s", __func__, strerror(errno));
#endif
    return ncpus;
}

uint32_t nm_hw_disk_free(void)
{
    uint32_t df = 0;
    struct statvfs fsstat;

    memset(&fsstat, 0, sizeof(fsstat));

    statvfs(nm_cfg_get()->vm_dir.data, &fsstat);
    df = (fsstat.f_bsize * fsstat.f_bavail) / 1024 / 1024 / 1024;

    return df;
}

/* vim:set ts=4 sw=4 fdm=marker: */
