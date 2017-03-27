#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_cfg_file.h>

#define NM_ADD_VM_FIELDS_NUM 11

void nm_add_vm(void)
{
    nm_window_t *w;
    nm_field_t *fields[NM_ADD_VM_FIELDS_NUM + 1];
    nm_form_t *form;
    nm_str_t buf = NM_INIT_STR;
    
    nm_print_title(_(NM_EDIT_TITLE));
    w = nm_init_window(25, 67, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(w, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_ADD_VM_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 38, n * 2, 5, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_ADD_VM_FIELDS_NUM] = NULL;

#if 0
    set_field_type(field[0], TYPE_REGEXP, "^[a-zA-Z0-9_-]* *$");
    set_field_type(field[1], TYPE_ENUM, ArchList, false, false);
    set_field_type(field[2], TYPE_INTEGER, 0, 1, cpu_count());
    set_field_type(field[3], TYPE_INTEGER, 0, 64, total_memory());
    set_field_type(field[4], TYPE_INTEGER, 0, 1, disk_free(vmdir_));
    set_field_type(field[5], TYPE_ENUM, (char **)drive_ints, false, false);
    set_field_type(field[6], TYPE_REGEXP, "^/.*");
    set_field_type(field[7], TYPE_INTEGER, 1, 0, 64);
    set_field_type(field[8], TYPE_ENUM, (char **)NetDrv, false, false);
    set_field_type(field[9], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[10], TYPE_ENUM, UdevList, false, false);
#endif

    set_field_type(fields[0], TYPE_REGEXP, "^[a-zA-Z0-9_-]* *$");
    set_field_type(fields[1], TYPE_ENUM, nm_cfg_get_arch(), false, false);
    set_field_type(fields[2], TYPE_INTEGER, 0, 1, 1);
    set_field_type(fields[3], TYPE_INTEGER, 0, 64, 1024);
    set_field_type(fields[4], TYPE_INTEGER, 0, 1, 10);
    set_field_type(fields[5], TYPE_ENUM, NULL, false, false);
    set_field_type(fields[6], TYPE_REGEXP, "^/.*");
    set_field_type(fields[7], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[8], TYPE_ENUM, NULL, false, false);
    set_field_type(fields[9], TYPE_ENUM, NULL, false, false);
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

    mvwaddstr(w, 2, 2, _("Name"));
    mvwaddstr(w, 4, 2, _("Architecture"));

    nm_str_alloc_text(&buf, _("CPU cores [1-2]"));
    mvwaddstr(w, 6, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Memory [64-1024]Mb"));
    mvwaddstr(w, 8, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Disk [1-10]Gb"));
    mvwaddstr(w, 10, 2, buf.data);
    nm_str_trunc(&buf, 0);

    mvwaddstr(w, 12, 2, _("Disk interface"));
    mvwaddstr(w, 14, 2, _("Path to ISO/IMG"));
    mvwaddstr(w, 16, 2, _("Network interfaces"));
    mvwaddstr(w, 18, 2, _("Net driver"));
    mvwaddstr(w, 20, 2, _("USB [yes/no]"));
    mvwaddstr(w, 22, 2, _("USB device"));

    form = nm_post_form(w, fields, 21);
    nm_draw_form(w, form);

    /* cleanup */
    nm_form_free(form, fields);
    nm_str_free(&buf);
}

/* vim:set ts=4 sw=4 fdm=marker: */
