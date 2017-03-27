#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>

void nm_form_free(nm_form_t *form, nm_field_t **fields)
{
    if (fields == NULL)
        nm_bug(_("%s NULL pointer fields"), __func__);
    
    unpost_form(form);
    free_form(form);

    for (; *fields; fields++)
        free_field(*fields);

    curs_set(0);
}

nm_form_t *nm_post_form(nm_window_t *w, nm_field_t **field, int begin_x)
{
    nm_form_t *form;
    int rows, cols; 

    form = new_form(field);
    if (form == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));

    getmaxyx(stdscr, rows, cols);
    scale_form(form, &rows, &cols);
    set_form_win(form, w);
    set_form_sub(form, derwin(w, rows, cols, 2, begin_x));
    box(w, 0, 0);
    post_form(form);

    return form;
}

void nm_draw_form(nm_window_t *w, nm_form_t *form)
{
    int confirm = 0;
    int ch;

    wtimeout(w, 500);

    while ((ch = wgetch(w)) != KEY_F(10))
    {
        if (confirm)
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
                /*std::string input = trim_field_buffer(field_buffer(current_field(form), 0), false);
                std::string result;

                if (append_path(input, result))
                {
                    set_field_buffer(current_field(form), 0, result.c_str());
                    form_driver(form, REQ_END_FIELD);
                }*/
            }
            break;

        case KEY_F(2):
            confirm = 1;
            form_driver(form, REQ_VALIDATION);
            break;

        default:
            form_driver(form, ch);
            break;
        }
    }
}

/* vim:set ts=4 sw=4 fdm=marker: */
