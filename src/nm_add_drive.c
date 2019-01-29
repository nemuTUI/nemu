#include <nm_core.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_hw_info.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_add_drive.h>
#include <nm_vm_control.h>

#define NM_DRIVE_FORM_MSG "Drive interface"
#define NM_DRIVE_SZ_START "Size [1-"
#define NM_DRIVE_SZ_END   "]Gb"

enum {
    NM_FLD_DRVSIZE = 0,
    NM_FLD_DRVTYPE,
    NM_FLD_COUNT
};

static void nm_add_drive_to_fs(const nm_str_t *name, const nm_str_t *size,
                               const nm_vect_t *drives);
static void nm_add_drive_to_db(const nm_str_t *name, const nm_str_t *size,
                               const nm_str_t *type, const nm_vect_t *drives);

void nm_add_drive(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_field_t *fields[NM_FLD_COUNT+ 1] = {NULL};
    nm_str_t buf = NM_INIT_STR;
    nm_str_t drv_size = NM_INIT_STR;
    nm_str_t drv_type = NM_INIT_STR;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_vect_t err = NM_INIT_VECT;
    nm_vect_t msg_fields = NM_INIT_VECT;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    size_t msg_len;

    nm_vmctl_get_data(name, &vm);
    if ((vm.drives.n_memb / 4) == NM_DRIVE_LIMIT)
    {
        nm_warn(_(NM_NSG_DRV_LIM));
        goto out;
    }

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_VDRIVE_ADD));
    nm_init_help_edit();

    nm_str_format(&buf, "%s%u%s",
        _(NM_DRIVE_SZ_START), nm_hw_disk_free(), _(NM_DRIVE_SZ_END));

    nm_vect_insert(&msg_fields, buf.data, buf.len + 1, NULL);
    nm_vect_insert(&msg_fields, _(NM_DRIVE_FORM_MSG),
            strlen(_(NM_DRIVE_FORM_MSG)) + 1, NULL);
    nm_vect_end_zero(&msg_fields);
    msg_len = nm_max_msg_len((const char **) msg_fields.data);

    if (nm_form_calc_size(msg_len, NM_FLD_COUNT, &form_data) != NM_OK)
        return;

    for (size_t n = 0; n < NM_FLD_COUNT; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    set_field_type(fields[NM_FLD_DRVSIZE], TYPE_INTEGER, 0, 1, nm_hw_disk_free());
    set_field_type(fields[NM_FLD_DRVTYPE], TYPE_ENUM, nm_form_drive_drv, false, false);

    set_field_buffer(fields[NM_FLD_DRVTYPE], 0, NM_DEFAULT_DRVINT);

    for (size_t n = 0, y = 1, x = 2; n < NM_FLD_COUNT; n++)
    {
        mvwaddstr(form_data.form_window, y, x, msg_fields.data[n]);
        y += 2;
    }

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[NM_FLD_DRVSIZE], &drv_size);
    nm_get_field_buf(fields[NM_FLD_DRVTYPE], &drv_type);
    nm_form_check_data(_("Size"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    nm_add_drive_to_fs(name, &drv_size, &vm.drives);
    nm_add_drive_to_db(name, &drv_size, &drv_type, &vm.drives);

out:
    NM_FORM_EXIT();
    nm_vect_free(&msg_fields, NULL);
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
    nm_str_free(&drv_size);
    nm_str_free(&drv_type);
    nm_str_free(&buf);
}

void nm_del_drive(const nm_str_t *name)
{
    int ch = 0, delete_drive = 0;
    nm_str_t query = NM_INIT_STR;
    nm_str_t drive_path = NM_INIT_STR;
    nm_vect_t drv_list = NM_INIT_VECT;
    nm_vect_t drives = NM_INIT_VECT;
    nm_menu_data_t m_drvs = NM_INIT_MENU_DATA;
    size_t drv_list_len = (getmaxy(side_window) - 4);
    size_t drv_count;

    nm_str_format(&query, NM_VM_GET_ADDDRIVES_SQL, name->data);

    nm_db_select(query.data, &drives);
    nm_str_trunc(&query, 0);

    if (drives.n_memb == 0)
    {
        nm_warn(_(NM_MSG_DRV_NONE));
        goto out;
    }

    drv_count = drives.n_memb / 2;

    werase(side_window);
    werase(action_window);
    werase(help_window);
    nm_init_side_drives();
    nm_init_help_delete();

    m_drvs.highlight = 1;
    if (drv_list_len < drv_count)
        m_drvs.item_last = drv_list_len;
    else
        m_drvs.item_last = drv_list_len = drv_count;

    for (size_t n = 0; n < drv_count; n++)
    {
        size_t idx_shift = 2 * n;
        nm_vect_insert(&drv_list,
                       nm_vect_str_ctx(&drives, idx_shift),
                       nm_vect_str_len(&drives, idx_shift) + 1,
                       NULL);
    }

    m_drvs.v = &drv_list;

    do {
        nm_menu_scroll(&m_drvs, drv_list_len, ch);

        if (ch == NM_KEY_ENTER)
        {
            delete_drive = 1;
            break;
        }

        nm_print_base_menu(&m_drvs);
        werase(action_window);
        nm_init_action(_(NM_MSG_VDRIVE_DEL));
        nm_print_drive_info(&drives, m_drvs.highlight);

        if (redraw_window)
        {
            nm_destroy_windows();
            endwin();
            refresh();
            nm_create_windows();
            nm_init_side_drives();
            nm_init_help_delete();
            nm_init_action(_(NM_MSG_VDRIVE_DEL));

            drv_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (drv_list_len < drv_count)
            {
                m_drvs.item_last = drv_list_len;
                m_drvs.item_first = 0;
                m_drvs.highlight = 1;
            }
            else
                m_drvs.item_last = drv_list_len = drv_count;

            redraw_window = 0;
        }
    } while ((ch = wgetch(action_window)) != NM_KEY_Q);

    if (!delete_drive)
        goto quit;

    nm_str_alloc_str(&drive_path, &nm_cfg_get()->vm_dir);
    nm_str_format(&drive_path, "/%s/%s", name->data,
            nm_vect_str_ctx(&drives, 2 * (m_drvs.highlight - 1)));

    if (unlink(drive_path.data) == -1)
        nm_warn(_(NM_MSG_DRV_EDEL));

    nm_str_format(&query,
        "DELETE FROM drives WHERE vm_name='%s' "
        "AND drive_name='%s'",
        name->data, nm_vect_str_ctx(&drives, 2 * (m_drvs.highlight - 1)));

quit:
    nm_db_edit(query.data);
    werase(side_window);
    werase(help_window);
    nm_init_side();
    nm_init_help_main();

out:
    nm_str_free(&query);
    nm_str_free(&drive_path);
    nm_vect_free(&drv_list, NULL);
    nm_vect_free(&drives, nm_str_vect_free_cb);
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
