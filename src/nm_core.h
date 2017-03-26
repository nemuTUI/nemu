#ifndef NM_CORE_H_
#define NM_CORE_H_

#if (NM_OS_LINUX)
#include <nm_os_linux_core.h>
#elif (NM_OS_FREEBSD)
#include <nm_os_freebsd_core.h>
#else
#error "Build on this system is not supported"
#endif

/* global external includes */
#include <curses.h>

/* global external typedefs */
/** ncurses **/
typedef WINDOW nm_window_t;

#define NM_OK   0
#define NM_ERR -1

#define NM_PROGNAME "nemu"
#define NM_VERSION  "1.0.0-dev"

#define _(S) gettext(S)
#define NM_STRING_NX(S) # S
#define NM_STRING(S) NM_STRING_NX(S)

#define NM_LOCALE "/share/locale" 

#ifndef NM_USR_PREFIX
#define NM_USR_PREFIX /usr
#endif

#define NM_LOCALE_PATH NM_STRING(NM_USR_PREFIX) NM_LOCALE

#define NM_DEFAULT_NETDRV "virtio"
#define NM_DEFAULT_DRVINT "virtio"

#define NM_MAIN_CHOICES 3
#define nm_arr_len(p) (sizeof(p) / sizeof((p)[0]))

#endif /* NM_CORE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
