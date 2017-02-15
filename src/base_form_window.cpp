#include <libintl.h>

#include "base_form_window.h"

namespace QManager {

void QMFormWindow::Delete_form() 
{
    unpost_form(form);
    free_form(form);

    for (size_t i = 0; i < field.size() - 1; ++i)
        free_field(field[i]);
}

void QMFormWindow::Draw_title()
{
    help_msg = _("F10 - finish, F2 - save");
    str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - str_size) / 2, help_msg.c_str());
    refresh();
    curs_set(1);
}

void QMFormWindow::Post_form(uint32_t size)
{
    form = new_form(&field[0]);
    scale_form(form, &row, &col);
    set_form_win(form, window);
    set_form_sub(form, derwin(window, row, col, 2, size));
    box(window, 0, 0);
    post_form(form);
}

void QMFormWindow::ExeptionExit(QMException &err)
{
    curs_set(0);
    PopupWarning Warn(err.what(), 3, 30, 4);
    Warn.Init();
    Warn.Print(Warn.window);
    refresh();
}

void QMFormWindow::Enable_color()
{
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));
}

void QMFormWindow::Gen_mac_address(struct guest_t<std::string> &guest, 
                                   uint32_t int_count,
                                   const std::string &vm_name)
{
    (void) guest;
    last_mac = std::stol(v_last_mac[0]);
    ifaces = gen_mac_addr(last_mac, int_count, vm_name);

    itm = ifaces.end();
    --itm;
    s_last_mac = itm->second;

    its = std::remove(s_last_mac.begin(), s_last_mac.end(), ':');
    s_last_mac.erase(its, s_last_mac.end());

    last_mac = std::stol(s_last_mac, 0, 16);
}

void QMFormWindow::Draw_form()
{
    while ((ch = wgetch(window)) != KEY_F(10))
    {
        switch(ch) {
        case KEY_DOWN:
            form_driver(form, REQ_VALIDATION);
            form_driver(form, REQ_NEXT_FIELD);
            form_driver(form, REQ_END_LINE);
            break;

        case KEY_UP:
            form_driver(form, REQ_VALIDATION);
            form_driver(form, REQ_PREV_FIELD);
            form_driver(form, REQ_END_LINE);
            break;

        case KEY_LEFT:
            if (field_type(current_field(form)) == TYPE_ENUM)
                form_driver(form, REQ_PREV_CHOICE);
            else
                form_driver(form, REQ_PREV_CHAR);
            break;

        case KEY_RIGHT:
            if (field_type(current_field(form)) == TYPE_ENUM)
                form_driver(form, REQ_NEXT_CHOICE);
            else
                form_driver(form, REQ_NEXT_CHAR);
            break;

        case KEY_BACKSPACE:
            form_driver(form, REQ_DEL_PREV);
            break;

        case KEY_F(2):
            form_driver(form, REQ_VALIDATION);
            break;

        default:
            form_driver(form, ch);
            break;
        }
    }
}

} // namespace QManager
