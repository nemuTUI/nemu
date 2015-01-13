#ifndef MISC_H_
#define MISC_H_

#include <sstream>

#include <boost/tokenizer.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>

#include "qemu_manage.h"

namespace QManager
{
  std::string trim_field_buffer(char *buff);
  MapString gen_mac_addr(uint64_t &mac, uint32_t &int_num, std::string &vm_name);
  MapString Gen_map_from_str(const std::string &str);
  MapStringVector Read_ifaces_from_json(const std::string &str);
  bool check_root();
  bool verify_mac(const std::string &mac);
  void spinner(uint32_t, uint32_t);
  void err_exit(const char *msg, const std::string &err = "");

  template <typename T>
  T read_cfg(const std::string &cfg, const char *param) {
    T value;
    try {
      boost::property_tree::ptree ptr;
      boost::property_tree::ini_parser::read_ini(cfg, ptr);
      value = ptr.get<T>(param);
    }
    catch(boost::property_tree::ptree_bad_path &err) {
      err_exit("Error parsing config file! missing parameter.", (std::string) err.what());
    }
    catch(boost::property_tree::ptree_bad_data &err) {
      err_exit("Error parsing config file! bad value.", (std::string) err.what());
    }
    catch(boost::property_tree::ini_parser::ini_parser_error &err) {
      err_exit("Config file not found!.", (std::string) err.what());
    }

    return value;
  }
}//namespace QManager

#endif
