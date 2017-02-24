#ifndef MISC_H_
#define MISC_H_

#include <sstream>

#include <boost/tokenizer.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "qemu_manage.h"

namespace QManager {

std::string trim_field_buffer(char *buff);
MapString gen_mac_addr(uint64_t &mac, uint32_t &int_num, const std::string &vm_name);
MapString Gen_map_from_str(const std::string &str);
MapStringVector Read_ifaces_from_json(const std::string &str);
bool check_root();
bool verify_mac(const std::string &mac);
void spinner(uint32_t, uint32_t);
void err_exit(const char *msg, const std::string &err = "");
bool append_path(const std::string &input, std::string &result);

const struct config *get_cfg();
void init_cfg();

} // namespace QManager

#endif
