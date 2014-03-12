#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <ncurses.h>
// debug
#include <fstream>
// end debug
#include "qemu_manage.h"

typedef unsigned int u_int;

static const std::string vmdir_regex = "^vmdir\\s*=\\s*\"(.*)\"";
static const std::string lmax_regex = "^list_max\\s*=\\s*\"(.*)\"";
static const std::string cfg = "test.cfg";
static const std::string dbf = "/var/db/qemu_manage2.db";
static const std::string sql_list_vm = "select name from vms order by name asc";

int main() {

  std::vector<std::string> choices;
  choices.push_back("Manage qemu VM");
  choices.push_back("Add qemu VM");
  choices.push_back("Show network map");

  u_int highlight(1);
  u_int ch;

  initscr();
  raw();
  noecho();
  curs_set(0);

  MainWindow main_window(10, 30);
  main_window.Init();

  std::string vmdir = get_param(cfg, vmdir_regex);

  for (;;) {
    u_int choice(0);
    main_window.Print();
    MenuList main_menu(main_window.window, highlight, choices);
    main_menu.Print();

    while((ch = wgetch(main_window.window))) {
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
        case 10:
          choice = highlight;
          break;
        case KEY_F(10):
          clear();
          refresh();
          endwin();
          exit(0);
          break;
      }

      MenuList main_menu(main_window.window, highlight, choices);
      main_menu.Print();

      if(choice != 0)
        break;
    }
    if(choice == 1) {
      QemuDb *db = new QemuDb(dbf);
      std::vector<std::string> res = db->SelectQuery(sql_list_vm);
      delete db;

      if(res.empty()) {
        // print warn
      } else {
        // do list guests
      }

      PopupWarning Warn("choice 1", 3, 15, 7, 31);
      Warn.Init();
      Warn.Print(Warn.window);
    } else if(choice == 2) {
      PopupWarning Warn("choice 2", 3, 15, 7, 31);
      Warn.Init();
      Warn.Print(Warn.window);
    } else if(choice == 3) {
      PopupWarning Warn("choice 3", 3, 15, 7, 31);
      Warn.Init();
      Warn.Print(Warn.window);
    }
  }

  return 0;
}
