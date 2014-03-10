#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <vector>
#include <ncurses.h>

std::string get_param(const std::string &cfg, const std::string &regex);

class TemplateWindow {
  public:
    TemplateWindow(int height, int width, int starty = 7, int startx = 25);
    void Init();
//    virtual void Print() = 0;
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

#endif
