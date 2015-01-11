#ifndef HOSTINFO_H_
#define HOSTINFO_H_

#include <fstream>
#include <thread>
#include <proc/sysinfo.h>
#include <sys/statvfs.h>
#include <boost/regex.hpp>

#include "qemu_manage.h"

namespace QManager
{
  MapString list_usb();
  VectorString list_arch();
  uint32_t total_memory();
  uint32_t disk_free(const std::string &vmdir);
  uint32_t cpu_count();
}// namespace QManager

#endif
