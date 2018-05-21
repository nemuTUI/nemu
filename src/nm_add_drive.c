#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_hw_info.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

#define NM_DRIVE_FIELDS_NUM 2

enum {
    NM_FLD_DRVSIZE = 0,
    NM_FLD_DRVTYPE,
};

static void nm_add_drive_to_fs(const nm_str_t *name, const nm_str_t *size,
                               const nm_vect_t *drives);
static void nm_add_drive_to_db(const nm_str_t *name, const nm_str_t *size,
                               const nm_str_t *type, const nm_vect_t *drives);

void nm_add_drive(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_field_t *fields[NM_DRIVE_FIELDS_NUM + 1];
    nm_window_t *window = NULL;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t drv_size = NM_INIT_STR;
    nm_str_t drv_type = NM_INIT_STR;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_vect_t err = NM_INIT_VECT;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_vmctl_get_data(name, &vm);

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(9, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_DRIVE_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 19, (n + 1) * 2, 1, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_DRIVE_FIELDS_NUM] = NULL;

    set_field_type(fields[NM_FLD_DRVSIZE], TYPE_INTEGER, 0, 1, nm_hw_disk_free());
    set_field_type(fields[NM_FLD_DRVTYPE], TYPE_ENUM, nm_form_drive_drv, false, false);

    set_field_buffer(fields[NM_FLD_DRVTYPE], 0, NM_DEFAULT_DRVINT);

    nm_str_alloc_text(&buf, _("Add disk to "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    nm_str_trunc(&buf, 0);
    nm_str_add_text(&buf, _("Disk [1-"));
    nm_str_format(&buf, "%u", nm_hw_disk_free());
    nm_str_add_text(&buf, "]Gb");
    mvwaddstr(window, 4, 2, buf.data);
    mvwaddstr(window, 6, 2, _("Disk interface"));

    form = nm_post_form(window, fields, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[NM_FLD_DRVSIZE], &drv_size);
    nm_get_field_buf(fields[NM_FLD_DRVTYPE], &drv_type);
    nm_form_check_data(_("Disk"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    if ((vm.drives.n_memb / 4) == 3 )
    {
        nm_print_warn(3, 2, _("3 disks limit reached"));
        goto out;
    }

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_add_drive_to_fs(name, &drv_size, &vm.drives);
    nm_add_drive_to_db(name, &drv_size, &drv_type, &vm.drives);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
    nm_str_free(&drv_size);
    nm_str_free(&drv_type);
    nm_str_free(&buf);
}

void nm_del_drive(const nm_str_t *name)
{
    nm_str_t query = NM_INIT_STR;
    nm_str_t drive_path = NM_INIT_STR;
    nm_vect_t v = NM_INIT_VECT;
    nm_vect_t drives = NM_INIT_VECT;
    const char *drive = NULL;

    nm_str_format(&query,
        "SELECT drive_name FROM drives WHERE vm_name='%s' "
        "AND boot='0'", name->data);

    nm_db_select(query.data, &v);
    nm_str_trunc(&query, 0);

    if (v.n_memb == 0)
    {
        nm_print_warn(3, 6, _("No additional disks"));
        goto out;
    }

    for (size_t n = 0; n < v.n_memb; n++)
    {
        nm_str_t *drive = (nm_str_t *) v.data[n];
        nm_vect_insert(&drives, drive->data, drive->len + 1, NULL);
    }

    nm_vect_end_zero(&drives);

    if ((drive = nm_form_select_drive(&drives)) == NULL)
    {
        nm_print_warn(3, 2, _("Incorrect disk name"));
        goto out;
    }

    nm_str_alloc_str(&drive_path, &nm_cfg_get()->vm_dir);
    nm_str_format(&drive_path, "/%s/%s", name->data, drive);

    if (unlink(drive_path.data) == -1)
        nm_print_warn(3, 2, _("Cannot delete drive from fylesystem"));

    nm_str_format(&query,
        "DELETE FROM drives WHERE vm_name='%s' "
        "AND drive_name='%s'",
        name->data, drive);

    nm_db_edit(query.data);

out:
    nm_str_free(&query);
    nm_str_free(&drive_path);
    nm_vect_free(&v, nm_str_vect_free_cb);
    nm_vect_free(&drives, NULL);
}

static void nm_add_drive_to_fs(const nm_str_t *name, const nm_str_t *size,
                               const nm_vect_t *drives)
{
    nm_str_t vm_dir = NM_INIT_STR;
    nm_str_t cmd = NM_INIT_STR;
    size_t drive_count = drives->n_memb / 4;
    char drv_ch = 'a' + drive_count;

    nm_str_copy(&vm_dir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vm_dir, '/');
    nm_str_add_str(&vm_dir, name);

    nm_str_alloc_text(&cmd, NM_STRING(NM_USR_PREFIX) "/bin/qemu-img create -f qcow2 ");
    nm_str_format(&cmd, "%s/%s_%c.img %sG",
        vm_dir.data, name->data, drv_ch, size->data);

    if (nm_spawn_process(&cmd, NULL) != NM_OK)
        nm_bug(_("%s: cannot create image file"), __func__);

    nm_str_free(&vm_dir);
    nm_str_free(&cmd);
}

static void nm_add_drive_to_db(const nm_str_t *name, const nm_str_t *size,
                               const nm_str_t *type, const nm_vect_t *drives)
{
    size_t drive_count = drives->n_memb / 4;
    char drv_ch = 'a' + drive_count;
    nm_str_t query = NM_INIT_STR;

    nm_str_add_text(&query, "INSERT INTO drives("
        "vm_name, drive_name, drive_drv, capacity, boot) VALUES('");
    nm_str_format(&query, "%s', '%s_%c.img', '%s', '%s', '0')",
        name->data, name->data, drv_ch, type->data, size->data);
    nm_db_edit(query.data);

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
