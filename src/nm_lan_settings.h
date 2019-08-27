#ifndef NM_LAN_SETTINGS_H_
#define NM_LAN_SETTINGS_H_

#include <nm_string.h>

void nm_lan_settings(void);
void nm_lan_create_veth(int info);
void nm_lan_parse_name(const nm_str_t *name, nm_str_t *ln, nm_str_t *rn);

#endif /* NM_LAN_SETTINGS_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
