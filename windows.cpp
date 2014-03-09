#include <ncurses.h>
#include "qemu_manage.h"

TemplateWindow::TemplateWindow(int height, int width, int starty, int startx) {
  height = height;
  width = width;
  starty = starty;
  startx = startx;
}

void TemplateWindow::Init() {
  clear();
  start_color();
  use_default_colors();
  main_window = newwin(height, width, starty, startx);
  keypad(main_window, TRUE);
  border(0,0,0,0,0,0,0,0);
}

void MainWindow::Print() {
  mvprintw(1, 1, "Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
  refresh();
}
