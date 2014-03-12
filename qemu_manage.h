#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <vector>
#include <ncurses.h>
#include <sqlite3.h>

std::string get_param(const std::string &cfg, const std::string &regex);

class TemplateWindow {
  public:
    TemplateWindow(int height, int width, int starty = 7, int startx = 25);
    void Init();
    WINDOW *window;

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
    MenuList(WINDOW *menu_window, u_int &highlight, const std::vector<std::string> &list);
    void Print();

  private:
    u_int highlight_;
    WINDOW *window_;
    std::vector<std::string> list_;
    int x, y;
};

class QemuDb {
  public:
    QemuDb(const std::string &dbf);
    std::vector<std::string> SelectQuery(const std::string &query);
    void ActionQuery(const std::string &query);
    ~QemuDb() { sqlite3_close(qdb); }

  private:
    sqlite3 *qdb;
    std::string dbf_, query_;
    std::vector<std::string> sql;
    int dbexec;
    char *zErrMsg;
};

class QMException : public std::exception {
  public:
    QMException(std::string m="exception!") : msg(m) {}
    ~QMException() throw() {}
    const char* what() const throw() { return msg.c_str(); }

  private:
    std::string msg;
};

#endif
