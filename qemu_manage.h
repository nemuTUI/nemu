#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <form.h>
#include <ncurses.h>
#include <sqlite3.h>

namespace QManager {

  typedef std::vector<std::string> VectorString;
  typedef std::map<std::string, std::string> MapString;

  enum input_choices {
    MenuVmlist = 1,
    MenuAddVm = 2,
#ifdef ENABLE_OPENVSWITCH
    MenuOvsMap = 3,
#endif
    MenuKeyEnter = 10,
  };

  std::string get_param(const std::string &cfg, const std::string &regex);
  MapString list_usb();
  VectorString list_arch();
  uint32_t total_memory();
  uint32_t disk_free(const std::string &vmdir);
  uint32_t cpu_count();

  std::string trim_field_buffer(char *buff);
  MapString gen_mac_addr(uint64_t &mac, uint32_t &int_num, std::string &vm_name);

  class TemplateWindow {
    public:
      TemplateWindow(int height, int width, int starty = 7, int startx = 25);
      void Init();
      WINDOW *window;
      ~TemplateWindow() { delwin(window); }

    protected:
      int row, col;
      int height_, width_;
      int startx_, starty_;
  };

  class MainWindow : public TemplateWindow {
    public:
      MainWindow(int height, int width, int starty = 7, int startx = 25)
        : TemplateWindow(height, width, starty, startx) {}
      void Print();
  };

  class VmWindow : public TemplateWindow {
    public:
      VmWindow(int height, int width, int starty = 7, int startx = 25)
        : TemplateWindow(height, width, starty, startx) {}
      void Print();
  };

  class VmInfoWindow : public TemplateWindow {
    public:
      VmInfoWindow(const std::string &guest, int height, int width, int starty = 7, int startx = 25);
      void Print();

    private:
      std::string guest_, title_;
  };

  class HelpWindow : public TemplateWindow {
    public:
      HelpWindow(int height, int width, int starty = 1, int startx = 1)
        : TemplateWindow(height, width, starty, startx) {}
      void Print();

    private:
      WINDOW *window_;
      std::vector<std::string> msg_;
      uint32_t line;
  };

  class AddVmWindow : public TemplateWindow {
    public:
      AddVmWindow(const std::string &dbf, const std::string &vmdir,
        int height, int width, int starty = 3, int startx = 3);
      void Print();

    private:
      void delete_form();

      std::string sql_s_last_vnc, sql_s_last_mac, sql_check_name,
      sql_u_last_mac, sql_u_last_vnc, sql_add_guest,
      dbf_, vmdir_, guest_dir, create_guest_dir_cmd, create_img_cmd,
      vm_name, vm_arch, vm_cpus, vm_memo, vm_disk, vm_vncp,
      vm_kvmf, vm_path, vm_ints, vm_usbp, vm_usbd, s_last_mac;

      uint32_t last_vnc, ui_vm_ints;
      uint64_t last_mac;

      VectorString v_last_vnc, v_last_mac,
      v_name;
      MapString ifaces;

      MapString::iterator itm;
      std::string::iterator its;

      FIELD *field[12];
      FORM *form;

      int ch, cmd_exit_status;
  };

  class PopupWarning : public TemplateWindow {
    public:
      PopupWarning(const std::string &msg, int height,
        int width, int starty = 7, int startx = 25);
        int Print(WINDOW *window);

    private:
      std::string msg_;
      WINDOW *window_;
      int ch_;
  };

  class MenuList {
    public:
      MenuList(WINDOW *menu_window, uint32_t &highlight);
      template <typename Iterator>
      void Print(Iterator begin, Iterator end) {
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);

        for(Iterator list = begin; list != end; ++list) {
          if (highlight_ ==  i + 1) {
            wattron(window_, A_REVERSE);
            mvwprintw(window_, y, x, "%s", list->c_str());
            wattroff(window_, A_REVERSE);
          } else {
            mvwprintw(window_, y, x, "%s", list->c_str());
          }
          y++; i++;
        }

        wrefresh(window_);
      }

    protected:
      uint32_t highlight_;
      WINDOW *window_;
      uint32_t x, y, i;
  };

  class VmList : public MenuList {
    public:
      VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir);
      template <typename Iterator>
      void Print(Iterator begin, Iterator end) {
        wattroff(window_, COLOR_PAIR(1));
        wattroff(window_, COLOR_PAIR(2));
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);

        for(Iterator list = begin; list != end; ++list) {
          lock_ = vmdir_ + "/" + *list + "/" + *list + ".lock";
          std::ifstream lock_f(lock_);

          if (lock_f) {
            vm_status.insert(std::make_pair(*list, "running"));
            wattron(window_, COLOR_PAIR(2));
          }
          else {
            vm_status.insert(std::make_pair(*list, "stopped"));
            wattron(window_, COLOR_PAIR(1));
          }

          if (highlight_ ==  i + 1) {
            wattron(window_, A_REVERSE);
            mvwprintw(window_, y, x, "%-20s%s", list->c_str(), vm_status.at(*list).c_str());
            wattroff(window_, A_REVERSE);
          }
          else {
            mvwprintw(window_, y, x, "%-20s%s", list->c_str(), vm_status.at(*list).c_str());
          }
          y++; i++;
          wrefresh(window_);
        }
      }
      MapString vm_status;

    private:
      std::string vmdir_, lock_;
  };

  class QemuDb {
    public:
      QemuDb(const std::string &dbf);
      VectorString SelectQuery(const std::string &query);
      void ActionQuery(const std::string &query);
      ~QemuDb() { sqlite3_close(qdb); }

    private:
      sqlite3 *qdb;
      std::string dbf_, query_;
      VectorString sql;
      int dbexec;
      char *zErrMsg;
  };

  class QMException : public std::exception {
    public:
      QMException(const std::string &m) : msg(m) {}
      ~QMException() throw() {}
      const char* what() const throw() { return msg.c_str(); }

    private:
      std::string msg;
  };

} // namespace QManager

#endif
