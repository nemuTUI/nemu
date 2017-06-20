#ifndef NM_NETWORK_H_
#define NM_NETWORK_H_

#include <nm_string.h>
#include <nm_vector.h>

#include <arpa/inet.h>

typedef struct {
    struct in_addr addr;
    uint32_t cidr;
} nm_net_addr_t;

int nm_net_iface_exists(const nm_str_t *name);
uint32_t nm_net_iface_idx(const nm_str_t *name);
#if defined (NM_OS_LINUX)
void nm_net_add_macvtap(const nm_str_t *name, const nm_str_t *parent,
                        const nm_str_t *maddr, int type);
void nm_net_del_iface(const nm_str_t *name);
#endif
void nm_net_add_tap(const nm_str_t *name);
void nm_net_del_tap(const nm_str_t *name);
void nm_net_set_ipaddr(const nm_str_t *name, const nm_str_t *addr);
void nm_net_mac_n2a(uint64_t maddr, nm_str_t *res);
int nm_net_verify_mac(const nm_str_t *mac);
int nm_net_verify_ipaddr4(const nm_str_t *src, nm_net_addr_t *net,
                          nm_str_t *err);

#define NM_INIT_NETADDR { {0}, 0 }

#endif /*NM_NETWORK_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
