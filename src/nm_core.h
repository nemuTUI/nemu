#ifndef NM_CORE_H_
#define NM_CORE_H_

#if defined (NM_OS_LINUX)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#elif defined (NM_OS_FREEBSD)
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#endif

#include <stdio.h>
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

#ifndef NM_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif
#include <assert.h>

#include <pwd.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <nm_cfg_file.h>
#include <nm_database.h>

#define NM_PROGNAME "nemu"

#ifndef NM_VERSION
#define NM_VERSION "v2.2.1"
#endif

#ifndef NM_USR_PREFIX
#define NM_USR_PREFIX /usr
#endif

#define NM_LOCALE "/share/locale"
#define NM_LOCALE_PATH NM_STRING(NM_USR_PREFIX) NM_LOCALE

#define NM_STRING_NX(S) # S
#define NM_STRING(S) NM_STRING_NX(S)

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

static const char NM_DEFAULT_NETDRV[] = "virtio-net-pci";
static const char NM_DEFAULT_DRVINT[] = "virtio";
static const char NM_DEFAULT_USBVER[] = "XHCI";
static const char NM_VM_PID_FILE[]    = "qemu.pid";
static const char NM_VM_QMP_FILE[]    = "qmp.sock";

static inline char * __attribute__((format_arg (1))) _(const char *str)
{
    return gettext(str);
}
static inline void nm_init_core()
{
    nm_cfg_init();
    nm_db_init();
}
static inline void nm_exit_core()
{
    nm_db_close();
    nm_cfg_free();
    exit(NM_OK);
}

#endif /* NM_CORE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
