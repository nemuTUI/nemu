#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>

std::string get_param(const std::string &cfg, const std::string &regex);

class TemplateWindow {
  public:
    TemplateWindow(int height, int width, int startx = 0, int starty = 0);
    void Init() const;
    virtual void Print() const = 0;

  private:
    int height, width;
    int startx, starty;
};

class MainWindow : TemplateWindow {
  public:
    MainWindow(int height, int width, int startx = 0, int starty = 0)
      : TemplateWindow(height, width, startx, starty) {}
    virtual void Print() const;
};

#endif
