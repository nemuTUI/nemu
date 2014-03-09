#include <iostream>
#include <string>
#include <vector>
#include <ncurses.h>

#include "qemu_manage.h"

static const std::string vmdir_regex = "^vmdir\\s*=\\s*\"(.*)\"";
static const std::string lmax_regex = "^list_max\\s*=\\s*\"(.*)\"";
static const std::string cfg = "test.cfg";

std::vector<std::string> choices;

int main() {
  using std::cout;
  using std::endl;

  choices.push_back("Manage qemu VM");
  choices.push_back("Add qemu VM");
  choices.push_back("Show network map");

  initscr();
  raw();
  noecho();
  curs_set(0);

  MainWindow main_window(30, 30);
  main_window.Print();

  std::string vmdir = get_param(cfg, vmdir_regex);

  return 0;
}
