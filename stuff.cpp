#include <sstream>
#include "qemu_manage.h"

std::string QManager::trim_field_buffer(char *buff) {
  std::string field(buff);
  field.erase(field.find_last_not_of(" ") + 1);

  return field;
}

QManager::MapString QManager::gen_mac_addr(uint64_t &mac, uint32_t &int_num, std::string &vm_name) {
  MapString ifaces;
  char buff[64];
  std::string if_name;

  for(uint32_t i=0, n=1; i<int_num; ++i, ++n) {
    uint64_t m = mac + n;
    int pos = 0;

    for(size_t byte = 0; byte < 6; ++byte) {
      uint32_t octet = ((m >> 40) & 0xff);

      pos += snprintf(buff + pos, sizeof(buff) - pos, "%02x:", octet);
      m <<= 8;
    }

    buff[--pos] = '\0';

    if_name = vm_name + "_eth" + std::to_string(i);

    ifaces.insert(std::make_pair(if_name, buff));
  }

  return ifaces;
}
