#ifndef NM_NETWORK_H_
#define NM_NETWORK_H_

#include <nm_string.h>
#include <nm_vector.h>

#include <arpa/inet.h>

typedef struct {
    struct in_addr addr;
    uint32_t cidr;
} nm_net_addr_t;

void nm_net_mac_to_str(uint64_t maddr, nm_str_t *res);
int nm_net_verify_mac(const nm_str_t *mac);
int nm_net_verify_ipaddr4(const nm_str_t *src, nm_net_addr_t *net,
                          nm_str_t *err);

#define NM_INIT_NETADDR { {0}, 0 }

#endif /*NM_NETWORK_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
