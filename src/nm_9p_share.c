#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

#define NM_9P_FIELDS_NUM 3

enum {
    NM_FLD_9PMODE = 0,
    NM_FLD_9PPATH,
    NM_FLD_9PNAME
};

void nm_9p_share(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_field_t *fields[NM_9P_FIELDS_NUM + 1];
    nm_window_t *window = NULL;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t err = NM_INIT_VECT;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_vmctl_get_data(name, &vm);

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(11, 51, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_9P_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 25, (n + 1) * 2, 1, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_9P_FIELDS_NUM] = NULL;

    set_field_type(fields[NM_FLD_9PMODE], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_9PPATH], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_9PNAME], TYPE_REGEXP, ".*");

    nm_str_alloc_text(&buf, _("Share files to "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Enable sharing"));
    mvwaddstr(window, 6, 2, _("Path to directory"));
    mvwaddstr(window, 8, 2, _("Name of the share"));

    form = nm_post_form(window, fields, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

out:
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
    nm_str_free(&buf);
}

/* vim:set ts=4 sw=4 fdm=marker: */
