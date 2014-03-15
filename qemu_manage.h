#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <ncurses.h>
#include <sqlite3.h>

namespace QManager {

  typedef std::vector<std::string> VectorString;

  enum input_choices {
    MenuVmlist = 1,
    MenuAddVm = 2,
#ifdef ENABLE_OPENVSWITCH
    MenuOvsMap = 3,
#endif
    MenuKeyEnter = 10,
  };

  std::string get_param(const std::string &cfg, const std::string &regex);

  class TemplateWindow {
    public:
      TemplateWindow(int height, int width, int starty = 7, int startx = 25);
      void Init();
      WINDOW *window;
      ~TemplateWindow() { delwin(window); }

    protected:
      int row, col;

    private:
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
      void Print(WINDOW *window);

    private:
      WINDOW *window_;
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
            vm_status.insert(std::pair<std::string, std::string>(*list, "running"));
            wattron(window_, COLOR_PAIR(2));
          }
          else {
            vm_status.insert(std::pair<std::string, std::string>(*list, "stopped"));
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
      std::map<std::string, std::string> vm_status;

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
