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
  char buff[1024];
  std::string regex(".*\\s+ID\\s+([A-Fa-f0-9]{4}:[A-Fa-f0-9]{4})\\s+(.*\\S)\\s*\n");

  FILE *cmd = popen("lsusb", "r");
  if(cmd) {
    while(char *line = fgets(buff, sizeof(buff), cmd)) {
      boost::regex re(regex, boost::regex_constants::extended);
      boost::smatch m;
      if (boost::regex_match(std::string(line), m, re)) {
        std::string usb_id = m[1].str();
        std::string usb_name = m[2].str();
        u_list.insert(std::make_pair(usb_name, usb_id));
      }
    }
    return u_list;
  }

  return MapString();
}

QManager::VectorString QManager::list_arch() {
  VectorString a_list;
  char buff[1024];

  FILE *cmd = popen(
    "ls -1 $(dirname $(which qemu-img))/qemu-system-* | sed -re 's/.*qemu-system-//'",
    "r"
  );
  if(cmd) {
    while(char *line = fgets(buff, sizeof(buff), cmd)) {
      strtok(line, "\n");
      a_list.push_back(line);
    }
    return a_list;
  }

  return VectorString();
}
