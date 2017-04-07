#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>

static nm_form_t *form = NULL;
static nm_field_t *fields[2];

void nm_clone_vm(const nm_str_t *name)
{
    nm_window_t *window = NULL;
    nm_str_t buf = NM_INIT_STR;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(7, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    fields[0] = new_field(1, 30, 2, 1, 0, 0);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_REGEXP, "^[a-zA-Z0-9_-]{1,30} *$");

    nm_str_alloc_str(&buf, name);
    nm_str_add_char(&buf, '_');
    set_field_buffer(fields[0], 0, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Clone "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Name"));
    nm_str_trunc(&buf, 0);

    form = nm_post_form(window, fields, 10);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    /* --- */

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_form_free(form, fields);
    nm_str_free(&buf);
}

/* vim:set ts=4 sw=4 fdm=marker: */
