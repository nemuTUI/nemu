#include <fstream> //debug
#include "qemu_manage.h"

void QManager::start_guest(
  const std::string &vm_name, const std::string &dbf, const std::string &vmdir
) {
  guest_t<VectorString> guest;

  std::string lock_file = vmdir + "/" + vm_name + "/" + vm_name + ".lock";

  std::ofstream debug;
  debug.open("/tmp/guest.log");
  debug << lock_file << std::endl;
  debug.close();
}
