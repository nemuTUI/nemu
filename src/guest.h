#ifndef GUEST_H_
#define GUEST_H_

#include "window.h"

namespace QManager {

void start_guest(const std::string &vm_name,
                 const std::string &dbf,
                 const std::string &vmdir,
                 struct start_data *data);

void delete_guest(const std::string &vm_name,
                  const std::string &dbf,
                  const std::string &vmdir);

void connect_guest(const std::string &vm_name, const std::string &dbf);
void kill_guest(const std::string &vm_name);
void clear_unused_tap_ifaces();

} // namespace QManager

#endif
/* vim:set ts=4 sw=4 fdm=marker: */
