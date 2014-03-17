#include <fstream>
#include <proc/sysinfo.h>
#include <boost/regex.hpp>

#include "qemu_manage.h"

std::string QManager::get_param(const std::string &cfg, const std::string &regex) {
  std::ifstream ifs(cfg.c_str());
  if (ifs.is_open()) {

    std::string line;
    while (std::getline(ifs, line)) {
      boost::regex re(regex);
      boost::smatch m;

      if (boost::regex_match(line, m, re)) {
        std::string param = m[1].str();
        return param;
      }
    }
  }
  return std::string();
}

uint32_t QManager::total_memory() {
  meminfo();
  return kb_main_total / 1024;
}
