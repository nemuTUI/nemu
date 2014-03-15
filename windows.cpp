#include <ncurses.h>
#include "qemu_manage.h"

QManager::TemplateWindow::TemplateWindow(int height, int width, int starty, int startx) {
  height_ = height;
  width_ = width;
  starty_ = starty;
  startx_ = startx;

  getmaxyx(stdscr, row, col);
}

void QManager::TemplateWindow::Init() {
  start_color();
  use_default_colors();
  window = newwin(height_, width_, starty_, startx_);
  keypad(window, TRUE);
}

void QManager::MainWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, 1, "Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
  refresh();
}

void QManager::VmWindow::Print() {
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, 1, "F1 - help, F10 - main menu");
  refresh();
}

QManager::VmInfoWindow::VmInfoWindow(const std::string &guest, int height, int width,
  int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    guest_ = guest;
    title_ = guest_ + " info";
}

void QManager::VmInfoWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - title_.size())/2, title_.c_str());
  getch();
  refresh();
  clear();
}

QManager::PopupWarning::PopupWarning(const std::string &msg, int height,
 int width, int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    msg_ = msg;
}

int QManager::PopupWarning::Print(WINDOW *window) {
  window_ = window;
  box(window_, 0, 0);
  mvwprintw(window_, 1, 1, "%s", msg_.c_str());
  wrefresh(window_);
  ch_ = wgetch(window_);

  return ch_;
}

QManager::MenuList::MenuList(WINDOW *window, uint32_t &highlight) {
  window_ = window;
  highlight_ = highlight;
}

QManager::VmList::VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir)
        : MenuList(menu_window, highlight) {
  vmdir_ = vmdir;
  init_pair(1, COLOR_WHITE, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
}
