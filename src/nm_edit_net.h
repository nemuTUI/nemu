#ifndef NM_EDIT_NET_H_
#define NM_EDIT_NET_H_

#include <nm_string.h>

typedef struct {
    nm_str_t name;
    nm_str_t drv;
    nm_str_t maddr;
    nm_str_t ipv4;
    nm_str_t netuser;
    nm_str_t hostfwd;
    nm_str_t smb;
#if defined (NM_OS_LINUX)
    nm_str_t vhost;
    nm_str_t macvtap;
    nm_str_t parent_eth;
    int tap_fd;
#endif
} nm_iface_t;

#if defined (NM_OS_LINUX)
#define NM_INIT_NET_IF (nm_iface_t) { \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR, NM_INIT_STR, -1 }
#else
#define NM_INIT_NET_IF (nm_iface_t) { \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR, NM_INIT_STR, \
                        NM_INIT_STR }
#endif


void nm_edit_net(const nm_str_t *name);

#endif /* NM_EDIT_NET_H_ */
/* vim:set ts=4 sw=4: */
