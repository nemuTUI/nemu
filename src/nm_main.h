#ifndef NM_MAIN_H_
#define NM_MAIN_H_

#include <locale.h>
#include <libintl.h>

#define NM_OK   0
#define NM_ERR -1

#define NM_PROGNAME "nemu"

#define _(S) gettext(S)
#define NM_STRING_NX(S) # S
#define NM_STRING(S) NM_STRING_NX(S)

#define NM_LOCALE "/share/locale" 

#ifndef NM_USR_PREFIX
#define NM_USR_PREFIX /usr
#endif

#define NM_LOCALE_PATH NM_STRING(NM_USR_PREFIX) NM_LOCALE

#define NM_VERSION "0.5.0-dev"
#define NM_DEFAULT_NETDRV "virtio"
#define NM_DEFAULT_DRVINT "virtio"

#endif /* NM_MAIN_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
