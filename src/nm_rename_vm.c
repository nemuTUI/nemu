#include <nm_core.h>
#include <nm_utils.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_rename_vm.h>

static const char NM_RENAME_FORM_MSG[] = "New VM name";

static void nm_rename_init_windows(nm_form_t *form);
static void nm_rename_vm_in_db(const nm_vmctl_data_t *vm, const nm_str_t *new_name);
static void nm_rename_vm_in_fs(const nm_vmctl_data_t *vm, const nm_str_t *new_name);

enum {
    NM_LBL_NEWNAME = 0, NM_FLD_NEWNAME,
    NM_FLD_COUNT
};

static void nm_rename_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
        if (form_data)
            form_data->parent_window = action_window;
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_RENAME_VM));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_rename_vm(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_field_t *fields[NM_FLD_COUNT + 1] = {NULL};
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t new_name = NM_INIT_STR;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_vect_t err = NM_INIT_VECT;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_rename_init_windows(NULL);

    nm_vmctl_get_data(name, &vm);

    msg_len = mbstowcs(NULL, _(NM_RENAME_FORM_MSG), strlen(_(NM_RENAME_FORM_MSG)));

    form_data = nm_form_data_new(
        action_window, nm_rename_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    fields[0] = nm_field_new(NM_FIELD_LABEL, 0, form_data);
    fields[1] = nm_field_new(NM_FIELD_EDIT, 0, form_data);
    fields[2] = NULL;

    set_field_buffer(fields[0], 0, _(NM_MSG_RENAME_VM));
    set_field_type(fields[1], TYPE_REGEXP, "^[a-zA-Z0-9_-]{1,30} *$");
    nm_str_format(&buf, "%s", name->data);
    set_field_buffer(fields[1], 0, buf.data);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[1], &new_name);
    nm_form_check_data(_(NM_RENAME_FORM_MSG), new_name, err);

    if (nm_print_empty_fields(&err) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    if (nm_str_cmp_ss(&new_name, name) == 0 || nm_form_name_used(&new_name) == NM_ERR)
        goto out;

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_vmctl_clear_tap(name);

    nm_db_begin_transaction();
    nm_rename_vm_in_db(&vm, &new_name);
    nm_rename_vm_in_fs(&vm, &new_name);
    nm_db_commit();

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    NM_FORM_EXIT();
    nm_vmctl_free_data(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_str_free(&buf);
    nm_str_free(&new_name);
}

static void nm_rename_vm_in_db(const nm_vmctl_data_t *vm, const nm_str_t *new_name)
{
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf_1 = NM_INIT_STR;
    nm_str_t buf_2 = NM_INIT_STR;
    nm_str_t *old_name = nm_vect_str(&vm->main, NM_SQL_NAME);

    size_t count = 0;

    // Update drives
    count = vm->drives.n_memb / NM_DRV_IDX_COUNT;
    for (size_t i = 0; i < count; i++) {
        size_t idx_shift = NM_DRV_IDX_COUNT * i;
        nm_str_t *old_drive_name = nm_vect_str(&vm->drives, NM_SQL_DRV_NAME + idx_shift);

        nm_str_copy(&buf_1, old_drive_name);
        nm_str_replace_text(&buf_1, old_name->data, new_name->data);

        nm_str_format(&query,
            "UPDATE drives SET vm_name = '%s', drive_name = '%s' "
                "WHERE vm_name = '%s' AND drive_name = '%s'",
            new_name->data, buf_1.data,
            old_name->data, old_drive_name->data);
        nm_db_atomic(query.data);
    }

    // Update ifaces
    count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;
    for (size_t i = 0; i < count; i++) {
        size_t idx_shift = NM_IFS_IDX_COUNT * i;
        nm_str_t *old_iface_name = nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift);
        nm_str_t *old_iface_altname = nm_vect_str(&vm->ifs, NM_SQL_IF_ALT + idx_shift);

        nm_str_copy(&buf_1, old_iface_name);
        nm_str_replace_text(&buf_1, old_name->data, new_name->data);
        nm_str_copy(&buf_2, old_iface_altname);
        nm_str_replace_text(&buf_2, old_name->data, new_name->data);

        nm_str_format(&query,
            "UPDATE ifaces SET vm_name = '%s', if_name = '%s', altname = '%s' "
                "WHERE if_name = '%s'",
            new_name->data,
            buf_1.data,
            buf_2.data,
            old_iface_name->data);
        nm_db_atomic(query.data);
    }

    // Update usb
    nm_str_format(&query,
        "UPDATE usb SET vm_name = '%s' WHERE vm_name = '%s'",
        new_name->data,
        old_name->data);
    nm_db_atomic(query.data);

    // Update vmsnapshots
    nm_str_format(&query,
        "UPDATE vmsnapshots SET vm_name = '%s' WHERE vm_name = '%s'",
        new_name->data,
        old_name->data);
    nm_db_atomic(query.data);

    // Update vms
    nm_str_format(&query,
        "UPDATE vms SET name = '%s' WHERE name = '%s'",
        new_name->data,
        old_name->data);
    nm_db_atomic(query.data);

    nm_str_free(&buf_1);
    nm_str_free(&buf_2);
    nm_str_free(&query);
}

//@TODO Make atomic? (copy on rename/track changes and undo on fail)
static void nm_rename_vm_in_fs(const nm_vmctl_data_t *vm, const nm_str_t *new_name)
{
    nm_str_t old_vm_dir = NM_INIT_STR;
    nm_str_t new_vm_dir = NM_INIT_STR;
    nm_str_t old_path = NM_INIT_STR;
    nm_str_t new_path = NM_INIT_STR;
    nm_str_t new_drive_name = NM_INIT_STR;
    size_t drives_count;
    struct stat stats;

    const nm_str_t *old_name = nm_vect_str(&vm->main, NM_SQL_NAME);

    nm_str_format(&old_vm_dir, "%s/%s",
        nm_cfg_get()->vm_dir.data, old_name->data);
    nm_str_format(&new_vm_dir, "%s/%s",
        nm_cfg_get()->vm_dir.data, new_name->data);

    if (stat(new_vm_dir.data, &stats) != -1)
       nm_bug(_("%s: path \"%s\" already exists"), __func__, new_vm_dir.data);
    else if (ENOENT != errno)
       nm_bug(_("%s: stat error: %s"), __func__, strerror(errno));

    if (rename(old_vm_dir.data, new_vm_dir.data) != 0)
        nm_bug(_("%s: rename error: %s"), __func__, strerror(errno));

    drives_count = vm->drives.n_memb / NM_DRV_IDX_COUNT;
    for (size_t i = 0; i < drives_count; i++)
    {
        size_t idx_shift = NM_DRV_IDX_COUNT * i;
        nm_str_t *old_drive_name = nm_vect_str(&vm->drives, NM_SQL_DRV_NAME + idx_shift);

        nm_str_format(&old_path, "%s/%s", new_vm_dir.data, old_drive_name->data);

        nm_str_copy(&new_drive_name, old_drive_name);
        nm_str_replace_text(&new_drive_name, old_name->data, new_name->data);
        nm_str_format(&new_path, "%s/%s", new_vm_dir.data, new_drive_name.data);

        if (rename(old_path.data, new_path.data) != 0)
            nm_bug(_("%s: rename error: %s"), __func__, strerror(errno));
    }

    nm_str_free(&old_vm_dir);
    nm_str_free(&new_vm_dir);
    nm_str_free(&old_path);
    nm_str_free(&new_path);
    nm_str_free(&new_drive_name);
}
