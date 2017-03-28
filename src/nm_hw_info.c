#include <nm_core.h>

#if (NM_OS_LINUX)
#include <sys/sysinfo.h>
#endif

uint32_t nm_hw_total_ram(void)
{
    uint32_t ram = 0;
#if (NM_OS_LINUX)
    struct sysinfo info;

    memset(&info, 0, sizeof(info));
    sysinfo(&info);
    ram = info.totalram / 1024 / 1024;
#endif

    return ram;
}

uint32_t nm_hw_ncpus(void)
{
    uint32_t ncpus = 0;
#if (NM_OS_LINUX)
    ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    return ncpus;
}

/* vim:set ts=4 sw=4 fdm=marker: */
