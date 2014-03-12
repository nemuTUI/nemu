#include <ncurses.h>
#include "qemu_manage.h"

TemplateWindow::TemplateWindow(int height, int width, int starty, int startx) {
  height_ = height;
  width_ = width;
  starty_ = starty;
  startx_ = startx;
}

void TemplateWindow::Init() {
  //clear();
  start_color();
  use_default_colors();
  window = newwin(height_, width_, starty_, startx_);
  keypad(window, TRUE);
}

void MainWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, 1, "Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
  refresh();
}

PopupWarning::PopupWarning(const std::string &msg, int height,
 int width, int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    msg_ = msg;
}

int PopupWarning::Print(WINDOW *window) {
  window_ = window;
  box(window_, 0, 0);
  mvwprintw(window_, 1, 1, "%s", msg_.c_str());
  wrefresh(window_);
  ch_ = wgetch(window_);
  //delwin(window_);

  return ch_;
}

MenuList::MenuList(WINDOW *window, u_int &highlight, const std::vector<std::string> &list) {
  window_ = window;
  highlight_ = highlight;
  list_ = list;
}

void MenuList::Print() {
  x = 2; y = 2;
  box(window_, 0, 0);
  for(u_int i(0); i < list_.size(); i++) {
    if (highlight_ == i + 1) {
      wattron(window_, A_REVERSE);
      mvwprintw(window_, y, x, "%s", list_[i].c_str());
      wattroff(window_, A_REVERSE);
    } else {
      mvwprintw(window_, y, x, "%s", list_[i].c_str());
    }
    y++;
  }
  wrefresh(window_);
}
