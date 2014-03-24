#include "qemu_manage.h"


void QManager::start_guest(const std::string &vm_name, const std::string &dbf) {
  guest_t<VectorString> guest;

  std::string lock_file = dbf + "/" + vm_name + "/" + vm_name + ".lock";

}
