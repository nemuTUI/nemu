#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_hw_info.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

void nm_add_drive(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_field_t *fields[2];
    nm_window_t *window = NULL;
    nm_str_t buf = NM_INIT_STR;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_vmctl_get_data(name, &vm);

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(7, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    fields[0] = new_field(1, 19, 2, 1, 0, 0);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_INTEGER, 0, 1, nm_hw_disk_free());

    nm_str_alloc_text(&buf, _("Add disk to "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    nm_str_trunc(&buf, 0);
    nm_str_add_text(&buf, _("Disk [1-"));
    nm_str_format(&buf, "%u", nm_hw_disk_free());
    nm_str_add_text(&buf, "]Gb");
    mvwaddstr(window, 4, 2, buf.data);

    form = nm_post_form(window, fields, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

out:
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
    nm_str_free(&buf);
}

/* vim:set ts=4 sw=4 fdm=marker: */
