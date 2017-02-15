#ifndef WINDOWS_H_
#define WINDOWS_H_

#include "qemu_manage.h"
#include "qemudb.h"
#include "misc.h"
#include "hostinfo.h"

namespace QManager {

class QMWindow
{
public:
    QMWindow(int height, int width, int starty = 7);
    void Init();
    virtual void Print() = 0;
    virtual ~QMWindow() {};

public:
    WINDOW *window;

protected:
    int row, col;
    int height_, width_;
    int startx_, starty_;
    std::string help_msg;
    int str_size;
};

class MainWindow : public QMWindow
{
public:
    MainWindow(int height, int width, int starty = 7)
    : QMWindow(height, width, starty) {}
    virtual void Print();
};

class VmWindow : public QMWindow
{
public:
    VmWindow(int height, int width, int starty = 7)
    : QMWindow(height, width, starty) {}
    virtual void Print();
};

class VmInfoWindow : public QMWindow
{
public:
    VmInfoWindow(const std::string &guest, const std::string &dbf,
                 int height, int width, int starty = 7);
    virtual void Print();

private:
    std::string guest_, title_, dbf_, sql_query;
    guest_t<VectorString> guest_info;
};

class HelpWindow : public QMWindow
{
public:
    HelpWindow(int height, int width, int starty = 1)
    : QMWindow(height, width, starty) {}
    virtual void Print();

private:
    WINDOW *window_;
    VectorString *msg_;
    uint32_t line;
};

class PopupWarning
{
public:
    PopupWarning(const std::string &msg, int height,
                 int width, int starty = 7);
    void Init();
    int Print(WINDOW *window);

    WINDOW *window;

private:
    std::string msg_;
    WINDOW *window_;
    int ch_;
    int row, col;
    int height_, width_;
    int startx_, starty_;
};

class MenuList
{
public:
    MenuList(WINDOW *menu_window, uint32_t &highlight);
    
    template <typename Iterator>
    void Print(Iterator begin, Iterator end)
    {
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);

        for (Iterator list = begin; list != end; ++list)
        {
            if (highlight_ ==  i + 1)
            {
                wattron(window_, A_REVERSE);
                mvwprintw(window_, y, x, "%s", list->c_str());
                wattroff(window_, A_REVERSE);
            } 
            else
                mvwprintw(window_, y, x, "%s", list->c_str());

            y++; i++;
        }

        wrefresh(window_);
    }

protected:
    uint32_t highlight_;
    WINDOW *window_;
    uint32_t x, y, i;
};

class VmList : public MenuList
{
public:
    VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir);
  
    template <typename Iterator>
    void Print(Iterator begin, Iterator end)
    {
        wattroff(window_, COLOR_PAIR(1));
        wattroff(window_, COLOR_PAIR(2));
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);

        for (Iterator list = begin; list != end; ++list)
        {
            lock_ = vmdir_ + "/" + *list + "/" + *list + ".lock";
            std::ifstream lock_f(lock_);

            if (lock_f)
            {
                vm_status.insert(std::make_pair(*list, "running"));
                wattron(window_, COLOR_PAIR(2));
            }
            else
            {
                vm_status.insert(std::make_pair(*list, "stopped"));
                wattron(window_, COLOR_PAIR(1));
            }

            if (highlight_ ==  i + 1)
            {
                wattron(window_, A_REVERSE);
                mvwprintw(window_, y, x, "%-20s%s", list->c_str(), vm_status.at(*list).c_str());
                wattroff(window_, A_REVERSE);
            }
            else
                mvwprintw(window_, y, x, "%-20s%s", list->c_str(), vm_status.at(*list).c_str());

            y++; i++;
            wrefresh(window_);
        }
    }
    
    MapString vm_status;

private:
    std::string vmdir_, lock_;
};

} // namespace QManager

#endif
