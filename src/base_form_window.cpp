#include <libintl.h>

#include "base_form_window.h"

namespace QManager {

void QMFormWindow::Delete_form() 
{
    unpost_form(form);
    free_form(form);

    for (size_t i = 0; i < field.size() - 1; ++i)
        free_field(field[i]);

    curs_set(0);
}

void QMFormWindow::Draw_title(const std::string &msg)
{
    str_size = mbstowcs(NULL, msg.c_str(), msg.size());

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - str_size) / 2, msg.c_str());
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

void QMFormWindow::ExceptionExit(QMException &err)
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

    if (int_count == 0)
        return;

    last_mac = std::stoull(v_last_mac[0]);
    ifaces = gen_mac_addr(last_mac, int_count, vm_name);

    itm = ifaces.end();
    --itm;
    s_last_mac = itm->second;

    its = std::remove(s_last_mac.begin(), s_last_mac.end(), ':');
    s_last_mac.erase(its, s_last_mac.end());

    last_mac = std::stoull(s_last_mac, 0, 16);
}

void QMFormWindow::Draw_form()
{
    action_ok = false;
    wtimeout(window, 500);

    while ((ch = wgetch(window)) != KEY_F(10))
    {
        if (action_ok)
            break;

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

        case 0x9: /* TAB KEY */
            if (field_type(current_field(form)) != TYPE_REGEXP)
                break;
            {
                form_driver(form, REQ_NEXT_FIELD);
                form_driver(form, REQ_PREV_FIELD);
                form_driver(form, REQ_END_FIELD);
                std::string input = trim_field_buffer(field_buffer(current_field(form), 0));
                std::string result;

                if (append_path(input, result))
                {
                    set_field_buffer(current_field(form), 0, result.c_str());
                    form_driver(form, REQ_END_FIELD);
                }
            }
            break;

        case KEY_F(2):
            action_ok = true;
            form_driver(form, REQ_VALIDATION);
            break;

        default:
            form_driver(form, ch);
            break;
        }
    }
}

} // namespace QManager
