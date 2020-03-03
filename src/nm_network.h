#ifndef NM_NETWORK_H_
#define NM_NETWORK_H_

#include <nm_string.h>
#include <nm_vector.h>

#include <arpa/inet.h>

typedef struct {
    struct in_addr addr;
    uint32_t cidr;
} nm_net_addr_t;

#define NM_INIT_NETADDR (nm_net_addr_t) { {0}, 0 }

int nm_net_iface_exists(const nm_str_t *name);
uint32_t nm_net_iface_idx(const nm_str_t *name);
#if defined (NM_OS_LINUX)
void nm_net_add_macvtap(const nm_str_t *name, const nm_str_t *parent,
                        const nm_str_t *maddr, int type);
void nm_net_del_iface(const nm_str_t *name);
void nm_net_add_veth(const nm_str_t *l_name, const nm_str_t *r_name);
void nm_net_link_up(const nm_str_t *name);
void nm_net_link_down(const nm_str_t *name);
int nm_net_link_status(const nm_str_t *name);
#endif
void nm_net_add_tap(const nm_str_t *name);
void nm_net_del_tap(const nm_str_t *name);
void nm_net_set_ipaddr(const nm_str_t *name, const nm_str_t *addr);
void nm_net_set_altname(const nm_str_t *name, const nm_str_t *altname);
int nm_net_fix_tap_name(nm_str_t *name, const nm_str_t *maddr);
void nm_net_mac_n2s(uint64_t maddr, nm_str_t *res);
uint64_t nm_net_mac_s2n(const nm_str_t *addr);
int nm_net_verify_mac(const nm_str_t *mac);
int nm_net_verify_ipaddr4(const nm_str_t *src, nm_net_addr_t *net,
                          nm_str_t *err);
int nm_net_check_port(const uint16_t port, const int type, const uint32_t inaddr);

#endif /*NM_NETWORK_H_ */
/* vim:set ts=4 sw=4: */
