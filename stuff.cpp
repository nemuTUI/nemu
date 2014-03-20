#include "qemu_manage.h"

std::string QManager::trim_field_buffer(char *buff) {
  std::string field(buff);
  field.erase(field.find_last_not_of(" ") + 1);

  return field;
}
