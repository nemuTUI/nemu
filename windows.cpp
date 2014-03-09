#include <ncurses.h>
#include "qemu_manage.h"

TemplateWindow::TemplateWindow(int height, int width, int starty, int startx) {
  height = height;
  width = width;
  starty = starty;
  startx = startx;
}

void TemplateWindow::Init() const {
  WINDOW *window;
  clear();
  start_color();
  use_default_colors();
  window = newwin(height, width, starty, startx);
  keypad(window, TRUE);
  border(0,0,0,0,0,0,0,0);
}

void MainWindow::Print() const {
  Init();
  mvprintw(1, 1, "Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
  refresh();
}
