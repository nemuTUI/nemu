#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <cstdint>
#include <vector>
#include <ncurses.h>
#include <sqlite3.h>

namespace QManager {

  typedef std::vector<std::string> VectorString;

  std::string get_param(const std::string &cfg, const std::string &regex);

  class TemplateWindow {
    public:
      TemplateWindow(int height, int width, int starty = 7, int startx = 25);
      void Init();
      WINDOW *window;
      ~TemplateWindow() { delwin(window); }

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
      void Print(Iterator begin, Iterator End) {
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);
        for(Iterator list = begin; list != End; ++list) {
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

    private:
      uint32_t highlight_;
      WINDOW *window_;
      uint32_t x, y, i;
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
