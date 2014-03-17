#include <iostream>
#include <cstdlib>
#include <string>
#include <array>
#include <vector>
#include <memory>
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

  std::unique_ptr<MainWindow> main_window(new MainWindow(10, 30));
  main_window->Init();

  const std::string vmdir = get_param(cfg, vmdir_regex);

  for (;;) {
    uint32_t choice(0);

    main_window->Print();
    std::unique_ptr<MenuList> main_menu(new MenuList(main_window->window, highlight));
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
          clear();
          refresh();
          endwin();
          exit(0);
          break;
      }

      std::unique_ptr<MenuList> main_menu(new MenuList(main_window->window, highlight));
      main_menu->Print(choices.begin(), choices.end());

      if(choice != 0)
        break;
    }
    if(choice == MenuVmlist) {

      std::unique_ptr<QemuDb> db(new QemuDb(dbf));
      VectorString guests = db->SelectQuery(sql_list_vm);

      if(guests.empty()) {
        std::unique_ptr<PopupWarning> Warn(new PopupWarning("No guests here.", 3, 20, 7, 31));
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

        clear();
        std::unique_ptr<VmWindow> vm_window(new VmWindow(listmax + 4, 32));
        vm_window->Init();
        vm_window->Print();

        uint32_t guest_first(0);
        uint32_t guest_last(listmax);

        uint32_t q_highlight(1);

        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
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

          else if((ch == KEY_DOWN) && (q_highlight == listmax)
            && (guest_last == guests.size())) {
            q_highlight = 1;
            guest_first = 0;
            guest_last = listmax;
          }

          else if(ch == KEY_DOWN) {
            if((q_highlight == guests.size()) && (guests.size() <= listmax))
              q_highlight = 1;
            else if((q_highlight == listmax) && (guest_last < guests.size())) {
              guest_first++;
              guest_last++;
            }
            else
              q_highlight++;
          }

          else if(ch == MenuKeyEnter) {
            std::string guest = guests.at((guest_first + q_highlight) - 1);
            std::unique_ptr<VmInfoWindow> vminfo_window(new VmInfoWindow(guest, 10, 30));
            vminfo_window->Print();
          }

          else if(ch == KEY_F(1)) {
            std::unique_ptr<HelpWindow> help_window(new HelpWindow(8, 40));
            help_window->Print();
          }

          else if(ch == KEY_F(10)) {
            refresh();
            endwin();
            break;
          }

          vm_window->Print();
          std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
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
