#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <ncurses.h>

std::string get_param(const std::string &cfg, const std::string &regex);

class TemplateWindow {
  public:
    TemplateWindow(int height, int width, int startx = 0, int starty = 0);
    void Init();
    virtual void Print() = 0;
    WINDOW *main_window;

  private:
    int height, width;
    int startx, starty;
};

class MainWindow : public TemplateWindow {
  public:
    MainWindow(int height, int width, int startx = 0, int starty = 0)
      : TemplateWindow(height, width, startx, starty) {}
    virtual void Print();
};

class MenuList {
  public:
    MenuList();
};

#endif
