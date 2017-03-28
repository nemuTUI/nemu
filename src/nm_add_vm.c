#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_hw_info.h>
#include <nm_cfg_file.h>

#define NM_ADD_VM_FIELDS_NUM 11

static nm_window_t *window = NULL;
static nm_form_t *form = NULL;

void nm_add_vm(void)
{
    nm_field_t *fields[NM_ADD_VM_FIELDS_NUM + 1];
    nm_str_t buf = NM_INIT_STR;
    
    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(25, 67, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_ADD_VM_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 38, n * 2, 5, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_ADD_VM_FIELDS_NUM] = NULL;

    set_field_type(fields[0], TYPE_REGEXP, "^[a-zA-Z0-9_-]* *$");
    set_field_type(fields[1], TYPE_ENUM, nm_cfg_get_arch(), false, false);
    set_field_type(fields[2], TYPE_INTEGER, 0, 1, nm_hw_ncpus());
    set_field_type(fields[3], TYPE_INTEGER, 0, 4, nm_hw_total_ram());
    set_field_type(fields[4], TYPE_INTEGER, 0, 1, 10);
    set_field_type(fields[5], TYPE_ENUM, nm_form_drive_drv, false, false);
    set_field_type(fields[6], TYPE_REGEXP, "^/.*");
    set_field_type(fields[7], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[8], TYPE_ENUM, nm_form_net_drv, false, false);
    set_field_type(fields[9], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[10], TYPE_ENUM, NULL, false, false);

    set_field_buffer(fields[1], 0, *nm_cfg_get()->qemu_targets.data);
    set_field_buffer(fields[2], 0, "1");
    set_field_buffer(fields[5], 0, NM_DEFAULT_DRVINT);
    set_field_buffer(fields[7], 0, "1");
    set_field_buffer(fields[8], 0, NM_DEFAULT_NETDRV);
    set_field_buffer(fields[9], 0, "no");
    field_opts_off(fields[0], O_STATIC);
    field_opts_off(fields[6], O_STATIC);
    field_opts_off(fields[10], O_STATIC);
    set_max_field(fields[0], 30);

    mvwaddstr(window, 2, 2, _("Name"));
    mvwaddstr(window, 4, 2, _("Architecture"));

    nm_str_alloc_text(&buf, _("CPU cores [1-"));
    nm_str_format(&buf, "%u", nm_hw_ncpus());
    nm_str_add_char(&buf, ']');
    mvwaddstr(window, 6, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Memory [4-"));
    nm_str_format(&buf, "%u", nm_hw_total_ram());
    nm_str_add_text(&buf, "]Mb");
    mvwaddstr(window, 8, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Disk [1-10]Gb"));
    mvwaddstr(window, 10, 2, buf.data);
    nm_str_trunc(&buf, 0);

    mvwaddstr(window, 12, 2, _("Disk interface"));
    mvwaddstr(window, 14, 2, _("Path to ISO/IMG"));
    mvwaddstr(window, 16, 2, _("Network interfaces"));
    mvwaddstr(window, 18, 2, _("Net driver"));
    mvwaddstr(window, 20, 2, _("USB [yes/no]"));
    mvwaddstr(window, 22, 2, _("USB device"));

    form = nm_post_form(window, fields, 21);
    nm_draw_form(window, form);

    /* cleanup */
    nm_form_free(form, fields);
    nm_str_free(&buf);
}

/* vim:set ts=4 sw=4 fdm=marker: */
