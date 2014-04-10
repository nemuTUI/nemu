#include <iostream>
#include <cstdlib>
#include <string>
#include <array>
#include <vector>
#include <memory>

#include "qemu_manage.h"

static const std::string cfg = "/etc/qemu_manage.cfg";
static const std::string sql_list_vm = "select name from vms order by name asc";

int main(int argc, char **argv) {

  using namespace QManager;

  // localization
  setlocale(LC_ALL,"");
  bindtextdomain("qemu-manager","/usr/share/locale");
  textdomain("qemu-manager");

#ifdef ENABLE_OPENVSWITCH
#define CHOICES_NUM 3
#else
#define CHOICES_NUM 2
#endif
  const std::array<std::string, CHOICES_NUM> choices = {
    _("Manage qemu VM"),
    _("Add qemu VM"),
#ifdef ENABLE_OPENVSWITCH
    _("Show network map")
#endif
  };
#undef CHOICES_NUM

  uint32_t highlight(1);
  uint32_t ch;

  initscr();
  raw();
  noecho();
  curs_set(0);

  std::unique_ptr<MainWindow> main_window(new MainWindow(10, 30));
  main_window->Init();

  const std::string vmdir = read_cfg<std::string>(cfg, "main.vmdir");
  const std::string dbf = read_cfg<std::string>(cfg, "main.db");

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
        std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("No guests here."), 3, 20, 7));
        Warn->Init();
        Warn->Print(Warn->window);
      }
      else {

        uint32_t listmax = read_cfg<uint32_t>(cfg, "main.list_max");

        if(listmax > guests.size())
          listmax = guests.size();

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
            std::unique_ptr<VmInfoWindow> vminfo_window(new VmInfoWindow(guest, dbf, 10, 30));
            vminfo_window->Print();
          }

          else if(ch == MenuKeyR) {
            std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
            vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

            std::string guest = guests.at((guest_first + q_highlight) - 1);

            if(vm_list->vm_status.at(guest) == "running") {
              std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Already running!"), 3, 20, 7));
              Warn->Init();
              Warn->Print(Warn->window);
            }
            else {

              if(! check_root()) {
                std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Must be root."), 3, 20, 7));
                Warn->Init();
                Warn->Print(Warn->window);
              }
              else {
                start_guest(guest, dbf, vmdir);
              }
            }
          }

          else if(ch == MenuKeyC) {
            std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
            vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

            std::string guest = guests.at((guest_first + q_highlight) - 1);

            if(vm_list->vm_status.at(guest) == "running") {
              connect_guest(guest, dbf);
            }
          }

          else if(ch == MenuKeyD) {
            std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
            vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

            std::string guest = guests.at((guest_first + q_highlight) - 1);

            if(vm_list->vm_status.at(guest) == "running") {
              std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Stop guest before!"), 3, 20, 7));
              Warn->Init();
              Warn->Print(Warn->window);
            }
            else {
              std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Proceed? (y/n)"), 3, 20, 7));
              Warn->Init();
              uint32_t ch = Warn->Print(Warn->window);

              if(ch == MenuKeyY) {
                delete_guest(guest, dbf, vmdir);
                // Exit from loop to reread guests
                break;
              }
            }
          }

          else if(ch == MenuKeyF) {
            std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
            vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

            std::string guest = guests.at((guest_first + q_highlight) - 1);

            if(vm_list->vm_status.at(guest) == "running") {
              kill_guest(guest);
            }
          }
          
          else if(ch == MenuKeyE) {
            std::string guest = guests.at((guest_first + q_highlight) - 1);

            std::unique_ptr<EditVmWindow> edit_window(new EditVmWindow(dbf, vmdir, guest, 17, 60));
            edit_window->Init();
            edit_window->Print();
          }

          else if(ch == MenuKeyL) {
            std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, vmdir));
            vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

            std::string guest = guests.at((guest_first + q_highlight) - 1);

            if(vm_list->vm_status.at(guest) == "running") {
              std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Stop guest before!"), 3, 20, 7));
              Warn->Init();
              Warn->Print(Warn->window);
            }
            else {
              std::unique_ptr<CloneVmWindow> clone_window(new CloneVmWindow(dbf, vmdir, guest, 7, 35));
              clone_window->Init();
              clone_window->Print();

              // Exit from loop to reread guests
              break;
            }
          }

          else if(ch == KEY_F(1)) {
            std::unique_ptr<HelpWindow> help_window(new HelpWindow(10, 40));
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
      std::unique_ptr<AddVmWindow> add_window(new AddVmWindow(dbf, vmdir, 25, 60));
      add_window->Init();
      add_window->Print();
    }
#ifdef ENABLE_OPENVSWITCH
    else if(choice == MenuOvsMap) {
      // do some stuff...
    }
#endif
  }

  return 0;
}
