#include <iostream>
#include <cstdlib>
#include <string>
#include <array>
#include <vector>
#include <ncurses.h>
// debug
#include <fstream>
// end debug
#include "qemu_manage.h"

static const std::string vmdir_regex = "^vmdir\\s*=\\s*\"(.*)\"";
static const std::string lmax_regex = "^list_max\\s*=\\s*\"(.*)\"";
static const std::string cfg = "test.cfg";
static const std::string dbf = "/var/db/qemu_manage2.db";
static const std::string sql_list_vm = "select name from vms order by name asc";

#ifdef ENABLE_OPENVSWITCH
#define CHOICES_NUM 3
#else
#define CHOICES_NUM 2
#endif
const std::array<std::string, CHOICES_NUM> choices = {
  "Manage qemu VM",
  "Add qemu VM",
#ifdef ENABLE_OPENVSWITCH
  "Show network map"
#endif
};
#undef CHOICES_NUM

int main() {

  using namespace QManager;

  uint32_t highlight(1);
  uint32_t ch;

  initscr();
  raw();
  noecho();
  curs_set(0);

  MainWindow *main_window = new MainWindow(10, 30);
  main_window->Init();

  std::string vmdir = get_param(cfg, vmdir_regex);

  for (;;) {
    uint32_t choice(0);

    main_window->Print();
    MenuList *main_menu = new MenuList(main_window->window, highlight);
    main_menu->Print(choices.begin(), choices.end());

    while((ch = wgetch(main_window->window))) {
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
          delete main_menu;
          delete main_window;
          clear();
          refresh();
          endwin();
          exit(0);
          break;
      }

      MenuList *main_menu = new MenuList(main_window->window, highlight);
      main_menu->Print(choices.begin(), choices.end());

      if(choice != 0)
        break;
    }
    if(choice == MenuVmlist) {

      QemuDb *db = new QemuDb(dbf);
      VectorString guests = db->SelectQuery(sql_list_vm);
      delete db;

      if(guests.empty()) {
        PopupWarning *Warn = new PopupWarning("No guests here.", 3, 20, 7, 31);
        Warn->Init();
        Warn->Print(Warn->window);
      }
      else {

        std::string listmax_s = get_param(cfg, lmax_regex);
        uint32_t listmax;

        if(listmax_s.empty()) {
          listmax = guests.size();
        }
        else {
          listmax = std::stoi(listmax_s); // TODO: fix parse cfg, check types.
          if(listmax > guests.size())
            listmax = guests.size();
        }

        VmWindow *vm_window = new VmWindow(listmax + 4, 32);
        vm_window->Init();
        vm_window->Print();

        uint32_t guest_first(0);
        uint32_t guest_last(listmax);

        uint32_t q_highlight(1); // TODO: try to replace it by highlight and delete.

        VmList *vm_list = new VmList(vm_window->window, q_highlight, vmdir);
        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

        for(;;) {
          ch = wgetch(vm_window->window);

          if((ch == KEY_UP) && (q_highlight == 1) && (guest_first == 0)
            && (listmax < guests.size())) {
            q_highlight = listmax;
            guest_first = guests.size() - listmax;
            guest_last = guests.size();
          }

          else if(ch == KEY_UP) {
            if((q_highlight == 1) && (guests.size() <= listmax))
              q_highlight = guests.size();
            else if((q_highlight == 1) && (guest_first != 0)) {
              guest_first--;
              guest_last--;
            }
            else
              q_highlight--;
          }

          else if(ch == KEY_F(10)) {
            delete vm_list;
            delete vm_window;
            refresh();
            endwin();
            break;
          }

          delete vm_list;
          //vm_window->Print();
          VmList *vm_list = new VmList(vm_window->window, q_highlight, vmdir);
          vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);
        }
      }

    }
    else if(choice == MenuAddVm) {
      // do some stuff...
    }
#ifdef ENABLE_OPENVSWITCH
    else if(choice == MenuOvsMap) {
      // do some stuff...
    }
#endif
  }

  return 0;
}
