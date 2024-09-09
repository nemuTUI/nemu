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

static const char NM_LC_DRIVE_FORM_MSG[] = "Drive interface";
static const char NM_LC_DRIVE_FORM_FORMAT[] = "Disk image format";
static const char NM_LC_DRIVE_FORM_DIS[] = "Discard mode";
static const char NM_LC_DRIVE_FORM_SZ_START[] = "Size [1-";
static const char NM_LC_DRIVE_FORM_SZ_END[]   = "]Gb";

static void nm_add_drive_init_windows(nm_form_t *form);
static size_t nm_add_drive_labels_setup(void);
static void nm_add_drive_to_db(const nm_str_t *name,
                               const nm_str_t *size,
                               const nm_str_t *type,
                               const nm_vect_t *drives,
                               const nm_str_t *discard,
                               const nm_str_t *format);

enum {
    NM_LBL_DRVSIZE = 0, NM_FLD_DRVSIZE,
    NM_LBL_DRVTYPE, NM_FLD_DRVTYPE,
    NM_LBL_FORMAT, NM_FLD_FORMAT,
    NM_LBL_DISCARD, NM_FLD_DISCARD,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_add_drive_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);

        if (form_data) {
            form_data->parent_window = action_window;
        }
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_VDRIVE_ADD));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_add_drive(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_str_t drv_size = NM_INIT_STR;
    nm_str_t drv_type = NM_INIT_STR;
    nm_str_t format = NM_INIT_STR;
    nm_str_t discard = NM_INIT_STR;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_vect_t err = NM_INIT_VECT;
    size_t msg_len;

    nm_vmctl_get_data(name, &vm);
    if ((vm.drives.n_memb / NM_DRV_IDX_COUNT) == NM_DRIVE_LIMIT) {
        nm_str_t warn_msg = NM_INIT_STR;

        nm_str_format(&warn_msg, _("%zu %s"), NM_DRIVE_LIMIT, NM_NSG_DRV_LIM);
        nm_warn(warn_msg.data);
        nm_str_free(&warn_msg);
        goto out;
    }

    nm_add_drive_init_windows(NULL);

    msg_len = nm_add_drive_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_add_drive_init_windows, msg_len,
        NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_FLD_DRVSIZE:
            fields[n] = nm_field_integer_new(
                n / 2, form_data, 0, 1, nm_hw_disk_free());
            break;
        case NM_FLD_DRVTYPE:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_drive_drv, false, false);
            break;
        case NM_FLD_FORMAT:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_drive_fmt, false, false);
            break;
        case NM_FLD_DISCARD:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        default:
            fields[n] = nm_field_label_new(n / 2, form_data);
            break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_add_drive_labels_setup();
    set_field_buffer(fields[NM_FLD_DRVTYPE], 0, NM_DEFAULT_DRVINT);
    set_field_buffer(fields[NM_FLD_FORMAT], 0, NM_DEFAULT_DRVFMT);
    set_field_buffer(fields[NM_FLD_DISCARD], 0, nm_form_yes_no[1]);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    nm_get_field_buf(fields[NM_FLD_DRVSIZE], &drv_size);
    nm_get_field_buf(fields[NM_FLD_DRVTYPE], &drv_type);
    nm_get_field_buf(fields[NM_FLD_FORMAT], &format);
    nm_get_field_buf(fields[NM_FLD_DISCARD], &discard);

    if (!drv_size.len) {
        nm_warn(_(NM_MSG_DRV_SIZE));
        goto out;
    }

    if (nm_print_empty_fields(&err) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    if (nm_add_drive_to_fs(name, &drv_size, &vm.drives, &format) != NM_OK) {
        nm_bug(_("%s: cannot create image file"), __func__);
    }
    nm_add_drive_to_db(name, &drv_size, &drv_type,
            &vm.drives, &discard, &format);

out:
    NM_FORM_EXIT();
    nm_vmctl_free_data(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_str_free(&drv_size);
    nm_str_free(&drv_type);
    nm_str_free(&discard);
}

static size_t nm_add_drive_labels_setup(void)
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_DRVSIZE:
            nm_str_format(&buf, "%s%u%s",
                _(NM_LC_DRIVE_FORM_SZ_START), nm_hw_disk_free(),
                _(NM_LC_DRIVE_FORM_SZ_END));
            break;
        case NM_LBL_DRVTYPE:
            nm_str_format(&buf, "%s", _(NM_LC_DRIVE_FORM_MSG));
            break;
        case NM_LBL_FORMAT:
            nm_str_format(&buf, "%s", _(NM_LC_DRIVE_FORM_FORMAT));
            break;
        case NM_LBL_DISCARD:
            nm_str_format(&buf, "%s", _(NM_LC_DRIVE_FORM_DIS));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len) {
            max_label_len = msg_len;
        }

        if (fields[n]) {
            set_field_buffer(fields[n], 0, buf.data);
        }
    }

    nm_str_free(&buf);
    return max_label_len;
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

    nm_str_format(&query, NM_SQL_DRIVES_SELECT_CAP, name->data);
    nm_db_select(query.data, &drives);

    if (drives.n_memb == 0) {
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
    if (drv_list_len < drv_count) {
        m_drvs.item_last = drv_list_len;
    } else {
        m_drvs.item_last = drv_list_len = drv_count;
    }

    for (size_t n = 0; n < drv_count; n++) {
        size_t idx_shift = 2 * n;

        nm_vect_insert(&drv_list,
                       nm_vect_str_ctx(&drives, idx_shift),
                       nm_vect_str_len(&drives, idx_shift) + 1,
                       NULL);
    }

    m_drvs.v = &drv_list;

    do {
        nm_menu_scroll(&m_drvs, drv_list_len, ch);

        if (ch == NM_KEY_ENTER) {
            delete_drive = 1;
            break;
        }

        nm_print_base_menu(&m_drvs);
        werase(action_window);
        nm_init_action(_(NM_MSG_VDRIVE_DEL));
        nm_print_drive_info(&drives, m_drvs.highlight);

        if (redraw_window) {
            nm_destroy_windows();
            endwin();
            refresh();
            nm_create_windows();
            nm_init_side_drives();
            nm_init_help_delete();
            nm_init_action(_(NM_MSG_VDRIVE_DEL));

            drv_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (drv_list_len < drv_count) {
                m_drvs.item_last = drv_list_len;
                m_drvs.item_first = 0;
                m_drvs.highlight = 1;
            } else {
                m_drvs.item_last = drv_list_len = drv_count;
            }

            redraw_window = 0;
        }
    } while ((ch = wgetch(action_window)) != NM_KEY_Q);

    if (!delete_drive) {
        goto quit;
    }

    nm_str_format(&drive_path, "%s/%s/%s",
        nm_cfg_get()->vm_dir.data, name->data,
        nm_vect_str_ctx(&drives, 2 * (m_drvs.highlight - 1)));

    if (unlink(drive_path.data) == -1) {
        nm_warn(_(NM_MSG_DRV_EDEL));
    }

    nm_str_format(&query, NM_SQL_DRIVES_DELETE,
        name->data, nm_vect_str_ctx(&drives, 2 * (m_drvs.highlight - 1)));

quit:
    nm_db_edit(query.data);
    werase(side_window);
    werase(help_window);
    nm_init_help_main();

out:
    nm_str_free(&query);
    nm_str_free(&drive_path);
    nm_vect_free(&drv_list, NULL);
    nm_vect_free(&drives, nm_str_vect_free_cb);
}

int nm_add_drive_to_fs(const nm_str_t *name, const nm_str_t *size,
    const nm_vect_t *drives, const nm_str_t *format)
{
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;

    nm_str_format(&buf, "%s/qemu-img", nm_cfg_get()->qemu_bin_path.data);
    nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

    nm_vect_insert_cstr(&argv, "create");
    nm_vect_insert_cstr(&argv, "-f");
    nm_vect_insert_cstr(&argv, format->data);

/*
 * @TODO Fix conversion from size_t to char
 * (might be a problem if there is too many drives)
 */
    size_t drive_count = 0;

    if (drives != NULL) {
        drive_count = drives->n_memb / NM_DRV_IDX_COUNT;
    }

    char drv_ch = 'a' + drive_count;

/* @TODO Why add VM name twice (in directory name and in filename)? */
    nm_str_format(&buf, "%s/%s/%s_%c.img",
        nm_cfg_get()->vm_dir.data, name->data, name->data, drv_ch);
    nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

    nm_str_format(&buf, "%sG", size->data);
    nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

    nm_vect_end_zero(&argv);
    if (nm_spawn_process(&argv, NULL) != NM_OK) {
        return NM_ERR;
    }

    nm_str_free(&buf);
    nm_vect_free(&argv, NULL);

    return NM_OK;
}

static void nm_add_drive_to_db(const nm_str_t *name, const nm_str_t *size,
                               const nm_str_t *type, const nm_vect_t *drives,
                               const nm_str_t *discard, const nm_str_t *format)
{
/*
 * @TODO Fix conversion from size_t to char
 * (might be a problem if there is too many drives)
 */
    size_t drive_count = drives->n_memb / NM_DRV_IDX_COUNT;
    char drv_ch = 'a' + drive_count;
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query, NM_SQL_DRIVES_INSERT_ADD,
        name->data, name->data, drv_ch, type->data, size->data,
        (nm_str_cmp_st(discard, "yes") == NM_OK) ? NM_ENABLE : NM_DISABLE,
        format->data);
    nm_db_edit(query.data);

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
