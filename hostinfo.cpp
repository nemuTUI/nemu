#include <fstream>
#include <thread>
#include <proc/sysinfo.h>
#include <sys/statvfs.h>
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

uint32_t QManager::disk_free(const std::string &vmdir) {
  struct statvfs df;
  statvfs(vmdir.c_str(), &df);

  return (df.f_bsize * df.f_bavail) / 1024 / 1024 / 1024;
}

uint32_t QManager::cpu_count() {
  uint32_t cores = std::thread::hardware_concurrency();

  return cores;
}

QManager::MapString QManager::list_usb() {
  MapString u_list;
  char line[1024];

  FILE *lsusb = popen("lsusb", "r");
  if(!lsusb)
    exit(1);

  while(fgets(line, sizeof(line), lsusb)) {
    //...
  }

  return MapString();
}
