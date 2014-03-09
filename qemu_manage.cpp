#include <iostream>
#include <string>
#include <vector>
#include <ncurses.h>

#include "qemu_manage.h"

typedef unsigned int u_int;

static const std::string vmdir_regex = "^vmdir\\s*=\\s*\"(.*)\"";
static const std::string lmax_regex = "^list_max\\s*=\\s*\"(.*)\"";
static const std::string cfg = "test.cfg";

std::vector<std::string> choices;

int main() {

  choices.push_back("Manage qemu VM");
  choices.push_back("Add qemu VM");
  choices.push_back("Show network map");

  int highlight(1);
  int ch;

  initscr();
  raw();
  noecho();
  curs_set(0);

  MainWindow main_window(10, 30);
  main_window.Init();
  main_window.Print();

  std::string vmdir = get_param(cfg, vmdir_regex);

  MenuList main_menu(main_window.window, highlight, choices);
  main_menu.Print();
 
  while((ch = wgetch(main_window.window)) != KEY_F(10)) {
    switch(ch) {
      case KEY_UP:
        if (highlight == 1)
          highlight = choices.size();
        else
          highlight--;
        break;
      case KEY_DOWN:
        if (highlight == choices.size())
          highlight = 1;
        else
          highlight++;
        break;
    }
    MenuList main_menu(main_window.window, highlight, choices);
    main_menu.Print();
  }

  endwin();
  return 0;
}
