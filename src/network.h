#ifndef NETWORK_H_
#define NETWORK_H_

namespace QManager {

bool net_add_tap(const std::string &name);
bool net_del_tap(const std::string &name);
bool net_set_ipaddr(const std::string &name, const std::string &addr);

} /* namespace QManager */
#endif /* NETWORK_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
