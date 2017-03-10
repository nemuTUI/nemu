#include <iostream>
#include <cstdlib>
#include <string>
#include <array>
#include <vector>
#include <memory>

#include <signal.h>
#include <libintl.h>

#include "nemu.h"
#include "qm_windows.h"
#include "guest.h"

using namespace QManager;

static void signals_handler(int signal);
static volatile sig_atomic_t redraw_window = 0;

int main(void)
{
    char usr_path[1024] = {0};
    snprintf(usr_path, sizeof(usr_path), "%s%s", STRING(USR_PREFIX), "/share/locale");

    setlocale(LC_ALL,"");
    bindtextdomain("nemu", usr_path);
    textdomain("nemu");

    init_cfg();
    const struct config *cfg = get_cfg();
    struct sigaction sa;

    QMWindow *main_window = NULL;
    QMWindow *vm_window = NULL;
    MenuList *main_menu = NULL;
    VmList *vm_list = NULL;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signals_handler;
    sigaction(SIGWINCH, &sa, NULL);

    const std::array<std::string, 3> choices = {
        _("Manage guests"),
        _("Install guest"),
        _("Quit"),
    };

    uint32_t highlight = 1;
    uint32_t ch, nemu = 0;

    initscr();
    raw();
    noecho();
    curs_set(0);

    for (;;)
    {
        uint32_t choice = 0;

        if (main_window)
            delete main_window;
        main_window = new MainWindow(10, 30);
        main_window->Init();
        main_window->Print();

        if (main_menu)
            delete main_menu;
        main_menu = new MenuList(main_window->window, highlight);
        main_menu->Print(choices.begin(), choices.end());

        while ((ch = wgetch(main_window->window)))
        {
            switch (ch) {
            case KEY_UP:
                if (highlight == 1)
                    highlight = choices.size();
                else
                    highlight--;
                break;

            case KEY_DOWN:
                if (highlight == choices.size())
                    highlight = 1;
                else
                    highlight++;
                break;

            case 10:
                choice = highlight;
                break;

            case KEY_F(10):
                clear();
                refresh();
                endwin();
                exit(0);
            }

            if (redraw_window)
            {
                endwin();
                refresh();
                if (main_window)
                    delete main_window;
                if (main_menu)
                    delete main_menu;
                main_window = new MainWindow(10, 30);
                main_window->Init();
                main_window->Print();
                main_menu = new MenuList(main_window->window, highlight);
                main_menu->Print(choices.begin(), choices.end());
                redraw_window = 0;
            }
            else
            {
                if (main_menu)
                    delete main_menu;
                main_menu = new MenuList(main_window->window, highlight);
                main_menu->Print(choices.begin(), choices.end());
            }

            if (choice != 0)
                break;
        }

        if (choice == MenuVmlist)
        {
            const std::string sql_list_vm = "select name from vms order by name asc";
            VectorString guests;

            std::unique_ptr<QemuDb> db(new QemuDb(cfg->db));
            db->SelectQuery(sql_list_vm, &guests);

            if (guests.empty())
            {
                std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("No guests here."), 3, 20, 7));
                Warn->Init();
                Warn->Print(Warn->window);
            }
            else
            {
                uint32_t listmax = cfg->list_max;
                uint32_t guest_first = 0;
                uint32_t guest_last;
                uint32_t q_highlight = 1;

                if (listmax > guests.size())
                    listmax = guests.size();

                guest_last = listmax;

                clear();
                if (vm_window)
                    delete vm_window;
                vm_window = new VmWindow(listmax + 4, 32);
                vm_window->Init();
                vm_window->Print();

                if (vm_list)
                    delete vm_list;
                vm_list = new VmList(vm_window->window, q_highlight, cfg->vmdir);
                vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                wtimeout(vm_window->window, 1000);

                for (;;)
                {
                    ch = wgetch(vm_window->window);

                    if ((ch == KEY_UP) && (q_highlight == 1) &&
                        (guest_first == 0) && (listmax < guests.size()))
                    {
                        q_highlight = listmax;
                        guest_first = guests.size() - listmax;
                        guest_last = guests.size();
                    }

                    else if (ch == KEY_UP)
                    {
                        if ((q_highlight == 1) && (guests.size() <= listmax))
                            q_highlight = guests.size();
                        else if ((q_highlight == 1) && (guest_first != 0))
                        {
                            guest_first--;
                            guest_last--;
                        }
                        else
                            q_highlight--;
                    }

                    else if ((ch == KEY_DOWN) && (q_highlight == listmax) &&
                             (guest_last == guests.size()))
                    {
                        q_highlight = 1;
                        guest_first = 0;
                        guest_last = listmax;
                    }

                    else if (ch == KEY_DOWN)
                    {
                        if ((q_highlight == guests.size()) && (guests.size() <= listmax))
                            q_highlight = 1;
                        else if ((q_highlight == listmax) && (guest_last < guests.size()))
                        {
                            guest_first++;
                            guest_last++;
                        }
                        else
                            q_highlight++;
                    }

                    else if (ch == MenuKeyEnter)
                    {
                        std::string guest = guests.at((guest_first + q_highlight) - 1);
                        std::unique_ptr<QMWindow> vminfo_window(new VmInfoWindow(guest, cfg->db, 10, 30));
                        vminfo_window->Print();
                    }

                    else if (ch == MenuKeyM)
                    {
                        std::string guest = guests.at((guest_first + q_highlight) - 1);
                        std::unique_ptr<QMWindow> cmdinfo_window(new CmdInfoWindow(guest, 10, 30));
                        cmdinfo_window->Print();
                    }

                    else if (ch == 0x6e || ch == 0x45 || ch == 0x4d || ch == 0x55)
                    {
                        if (ch == 0x6e && !nemu)
                             nemu++;
                        if (ch == 0x45 && nemu == 1)
                             nemu++;
                        if (ch == 0x4d && nemu == 2)
                             nemu++;
                        if (ch == 0x55 && nemu == 3)
                        {
                            std::unique_ptr<NemuWindow> nemu_window(new NemuWindow(10, 30));
                            nemu_window->Print();
                            nemu = 0;
                        }
                    }

                    else if (ch == MenuKeyR)
                    {
                        struct start_data start;
                        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, cfg->vmdir));
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        if (vm_list->vm_status.at(guest) == "running")
                        {
                            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Already running!"), 3, 20, 7));
                            Warn->Init();
                            Warn->Print(Warn->window);
                        }
                        else
                            start_guest(guest, cfg->db, cfg->vmdir, &start);
                    }

                    else if (ch == MenuKeyT)
                    {
                        struct start_data start;
                        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, cfg->vmdir));
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        if (vm_list->vm_status.at(guest) == "running")
                        {
                            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Already running!"), 3, 20, 7));
                            Warn->Init();
                            Warn->Print(Warn->window);
                        }
                        else
                        {
                            start.flags |= START_TEMP;
                            start_guest(guest, cfg->db, cfg->vmdir, &start);
                        }
                    }

                    else if (ch == MenuKeyC)
                    {
                        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, cfg->vmdir));
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        if (vm_list->vm_status.at(guest) == "running")
                            connect_guest(guest, cfg->db);
                    }

                    else if (ch == MenuKeyD)
                    {
                        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, cfg->vmdir));
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        if (vm_list->vm_status.at(guest) == "running")
                        {
                            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Stop guest before!"), 3, 20, 7));
                            Warn->Init();
                            Warn->Print(Warn->window);
                        }
                        else
                        {
                            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Proceed? (y/n)"), 3, 20, 7));
                            Warn->Init();
                            uint32_t ch = Warn->Print(Warn->window);

                            if (ch == MenuKeyY)
                            {
                                delete_guest(guest, cfg->db, cfg->vmdir);
                                // Exit from loop to reread guests
                                break;
                            }
                        }
                    }

                    else if (ch == MenuKeyF)
                    {
                        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, cfg->vmdir));
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        if (vm_list->vm_status.at(guest) == "running")
                            kill_guest(guest);
                    }
     
                    else if (ch == MenuKeyE)
                    {
                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        std::unique_ptr<QMFormWindow> edit_window(new EditVmWindow(cfg->db, cfg->vmdir, guest, 23, 67));
                        edit_window->Init();
                        edit_window->Print();
                    }

                    else if (ch == MenuKeyA)
                    {
                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        std::unique_ptr<QMFormWindow> add_disk_window(new AddDiskWindow(cfg->db, cfg->vmdir, guest, 7, 35));
                        add_disk_window->Init();
                        add_disk_window->Print();
                    }

                    else if (ch == MenuKeyI)
                    {
                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        std::unique_ptr<QMFormWindow> edit_net_window(new EditNetWindow(cfg->db, cfg->vmdir, guest, 9, 39));
                        edit_net_window->Init();
                        edit_net_window->Print();
                    }

                    else if (ch == MenuKeyS)
                    {
                        const std::string guest = guests.at((guest_first + q_highlight) - 1);

                        std::unique_ptr<QMFormWindow> edit_install_window(new EditInstallWindow(cfg->db, cfg->vmdir, guest, 17, 67));
                        edit_install_window->Init();
                        edit_install_window->Print();
                    }

                    else if (ch == MenuKeyL)
                    {
                        std::unique_ptr<VmList> vm_list(new VmList(vm_window->window, q_highlight, cfg->vmdir));
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);

                        std::string guest = guests.at((guest_first + q_highlight) - 1);

                        if (vm_list->vm_status.at(guest) == "running")
                        {
                            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Stop guest before!"), 3, 20, 7));
                            Warn->Init();
                            Warn->Print(Warn->window);
                        }
                        else
                        {
                            std::unique_ptr<QMFormWindow> clone_window(new CloneVmWindow(cfg->db, cfg->vmdir, guest, 7, 35));
                            clone_window->Init();
                            clone_window->Print();

                            // Exit from loop to reread guests
                            break;
                        }
                    }

                    else if (ch == KEY_F(1))
                    {
                        std::unique_ptr<QMWindow> help_window(new HelpWindow(15, 40));
                        help_window->Init();
                        help_window->Print();
                    }

                    else if (ch == KEY_F(10))
                    {
                        if (vm_window)
                        {
                            delete vm_window;
                            vm_window = NULL;
                        }
                        if (vm_list)
                        {
                            delete vm_list;
                            vm_list = NULL;
                        }
                        refresh();
                        endwin();
                        break;
                    }
                    vm_window->Print();

                    if (redraw_window)
                    {
                        clear();
                        endwin();
                        refresh();
                        if (vm_window)
                            delete vm_window;
                        if (vm_list)
                            delete vm_list;
                        vm_window = new VmWindow(listmax + 4, 32);
                        vm_window->Init();
                        vm_window->Print();
                        wtimeout(vm_window->window, 1000);
                        vm_list = new VmList(vm_window->window, q_highlight, cfg->vmdir);
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);
                        redraw_window = 0;
                    }
                    else
                    {
                        if (vm_list)
                            delete vm_list;
                        vm_list = new VmList(vm_window->window, q_highlight, cfg->vmdir);
                        vm_list->Print(guests.begin() + guest_first, guests.begin() + guest_last);
                    }
                }
            }
        }
        else if (choice == MenuAddVm)
        {
            std::unique_ptr<QMFormWindow> add_window(new AddVmWindow(cfg->db, cfg->vmdir, 25, 67));
            add_window->Init();
            add_window->Print();
        }
        else if (choice == MenuQuit)
        {
            clear();
            refresh();
            endwin();
            exit(0);
        }
    }

    return 0;
}

static void signals_handler(int signal)
{
    switch (signal) {
    case SIGWINCH:
        redraw_window = 1;
        break;
    }
}
