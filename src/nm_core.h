#ifndef NM_CORE_H_
#define NM_CORE_H_

#if defined (NM_OS_LINUX)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
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
#include <libintl.h>
#include <fcntl.h>
#include <inttypes.h>
#include <regex.h>
#include <getopt.h>

#include <pwd.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#define NM_PROGNAME "nemu"
#define NM_VERSION  "1.2.0-dev"

#define NM_OK   0
#define NM_ERR -1

#define NM_ENABLE  "1"
#define NM_DISABLE "0"

#define _(S) gettext(S)
#define NM_STRING_NX(S) # S
#define NM_STRING(S) NM_STRING_NX(S)

#define NM_LOCALE "/share/locale" 

#ifndef NM_USR_PREFIX
#define NM_USR_PREFIX /usr
#endif

#define NM_LOCALE_PATH NM_STRING(NM_USR_PREFIX) NM_LOCALE

#define NM_DEFAULT_NETDRV "virtio-net-pci"
#define NM_DEFAULT_DRVINT "virtio"
#define NM_VM_PID_FILE "qemu.pid"
#define NM_VM_QMP_FILE "qmp.sock"

#define NM_MAIN_CHOICES 3
#define nm_arr_len(p) (sizeof(p) / sizeof((p)[0]))

#endif /* NM_CORE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
