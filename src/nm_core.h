#ifndef NM_CORE_H_
#define NM_CORE_H_

#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <libintl.h>
#include <fcntl.h>
#include <inttypes.h>
#include <regex.h>
#include <getopt.h>
#include <limits.h>

#include <pwd.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <nm_cfg_file.h>
#include <nm_database.h>

#define NM_PROGNAME "nemu"

#ifndef NM_VERSION
#define NM_VERSION "v3.0.0"
#endif

#define nm_min(a, b) \
    __extension__({ \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a < _b ? _a : _b; \
    })
#define nm_max(a, b) \
    __extension__({ \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a > _b ? _a : _b; \
    })
#define nm_arr_len(p) (sizeof(p) / sizeof((p)[0]))

#define NM_UNUSED __attribute__((__unused__))

static const int NM_OK  = 0;
static const int NM_ERR = -1;

static const bool NM_TRUE  = true;
static const bool NM_FALSE = false;

static const char NM_ENABLE[]  = "1";
static const char NM_DISABLE[] = "0";

static const char NM_DEFAULT_NETDRV[]  = "virtio-net-pci";
static const char NM_DEFAULT_DRVINT[]  = "virtio";
static const char NM_DEFAULT_USBVER[]  = "XHCI";
static const char NM_VM_PID_FILE[]     = "qemu.pid";
static const char NM_VM_QMP_FILE[]     = "qmp.sock";
static const char NM_DEFAULT_DISPLAY[] = "qxl";
static const char NM_MQ_PATH[]         = "/nemu-qmp";

static inline char * __attribute__((format_arg (1))) _(const char *str)
{
    return gettext(str);
}
static inline void nm_init_core()
{
    nm_cfg_init();
    nm_db_init();
}
static inline void __attribute__((noreturn)) nm_exit_core()
{
    if (nm_db_in_transaction())
        nm_db_rollback();

    nm_db_close();
    nm_cfg_free();
    exit(NM_OK);
}
static inline int compar_uint32_t(const void *a, const void *b)
{
    const uint32_t _a = *(const uint32_t*)a;
    const uint32_t _b = *(const uint32_t*)b;
    if (_a != _b)
        return _a < _b ? -1 : 1;

    return 0;
}

#endif /* NM_CORE_H_ */
/* vim:set ts=4 sw=4: */
