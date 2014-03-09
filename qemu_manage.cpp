#include <iostream>
#include <string>

#include "qemu_manage.h"

static const std::string vmdir_regex = "^vmdir\\s*=\\s*\"(.*)\"";
static const std::string lmax_regex = "^list_max\\s*=\\s*\"(.*)\"";
static const std::string cfg = "test.cfg";

int main() {
  using std::cout;
  using std::endl;

  std::string vmdir = get_param(cfg, vmdir_regex);
  cout << vmdir << endl;

  return 0;
}
