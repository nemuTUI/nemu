#ifndef NM_OS_LINUX_CORE_H_
#define NM_OS_LINUX_CORE_H_

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* sigaction() */
#endif

#include <stdio.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>
#include <fcntl.h>
#include <libgen.h>
#include <inttypes.h>

#include <pwd.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#endif /* NM_OS_LINUX_CORE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
