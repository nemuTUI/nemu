#ifndef NM_NETWORK_H_
#define NM_NETWORK_H_

#include <nm_string.h>
#include <nm_vector.h>

void nm_net_mac_to_str(uint64_t maddr, nm_str_t *res);
int nm_net_verify_mac(const nm_str_t *mac);

#endif /*NM_NETWORK_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
