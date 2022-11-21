#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_qmp_control.h>
#include <nm_vm_snapshot.h>

#if !defined(NM_OS_DARWIN)

typedef struct {
    nm_str_t snap_name;
    nm_str_t load;
    int update;
} nm_vmsnap_t;

#define NM_INIT_VMSNAP (nm_vmsnap_t) { NM_INIT_STR, NM_INIT_STR, 0 }

static const char NM_LC_VMSNAP_FORM_NAME[] = "Snapshot name";
static const char NM_LC_VMSNAP_FORM_LOAD[] = "Load at next boot";
static const char NM_LC_VMSNAP_FORM_SNAP[] = "Snapshot";

static void nm_vm_snapshot_init_windows(nm_form_t *form);
static void nm_vm_snapshot_create_init_windows(nm_form_t *form);
static void nm_vm_snapshot_revert_init_windows(nm_form_t *form);
static void nm_vm_snapshot_delete_init_windows(nm_form_t *form);
static void nm_vm_snapshot_create_fields_setup(void);
static size_t nm_vm_snapshot_create_labels_setup(void);
static int nm_vm_snapshot_get_data(const nm_str_t *name, nm_vmsnap_t *data);
static void nm_vm_snapshot_to_db(const nm_str_t *name, const nm_vmsnap_t *data);
static void __nm_vm_snapshot_load(const nm_str_t *name, const nm_str_t *snap,
                                  int vm_status);
static void __nm_vm_snapshot_delete(const nm_str_t *name, const nm_str_t *snap,
                                    int vm_status);

enum {
    NM_SQL_VMSNAP_ID = 0,
    NM_SQL_VMSNAP_VM,
    NM_SQL_VMSNAP_NAME,
    NM_SQL_VMSNAP_LOAD,
    NM_SQL_VMSNAP_TIME
};

enum {
    NM_LBL_VMSNAPNAME = 0, NM_FLD_VMSNAPNAME,
    NM_LBL_VMLOAD, NM_FLD_VMLOAD,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];
static nm_vect_t snaps = NM_INIT_VECT;

static void nm_vm_snapshot_init_windows(nm_form_t *form)
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
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

static void nm_vm_snapshot_create_init_windows(nm_form_t *form)
{
    nm_vm_snapshot_init_windows(form);
    nm_init_action(_(NM_MSG_SNAP_CRT));
}

static void nm_vm_snapshot_revert_init_windows(nm_form_t *form)
{
    nm_vm_snapshot_init_windows(form);
    nm_init_action(_(NM_MSG_SNAP_REV));
    nm_print_snapshots(&snaps);
}

static void nm_vm_snapshot_delete_init_windows(nm_form_t *form)
{
    nm_vm_snapshot_init_windows(form);
    nm_init_action(_(NM_MSG_SNAP_DEL));
    nm_print_snapshots(&snaps);
}

void nm_vm_snapshot_create(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_vmsnap_t data = NM_INIT_VMSNAP;
    pthread_t spin_th;
    int done = 0;
    size_t msg_len;

    nm_vm_snapshot_create_init_windows(NULL);

    msg_len = nm_vm_snapshot_create_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_vm_snapshot_create_init_windows,
        msg_len, NM_FLD_COUNT / 2, NM_TRUE
    );

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_FLD_VMSNAPNAME:
            fields[n] = nm_field_default_new(n / 2, form_data);
            break;
        case NM_FLD_VMLOAD:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        default:
            fields[n] = nm_field_label_new(n / 2, form_data);
            break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_vm_snapshot_create_labels_setup();
    nm_vm_snapshot_create_fields_setup();
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    if (nm_vm_snapshot_get_data(name, &data) != NM_OK) {
        goto out;
    }

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar,
                (void *) &sp_data) != 0) {
        nm_bug(_("%s: cannot create thread"), __func__);
    }

    if (nm_qmp_savevm(name, &data.snap_name) != NM_ERR) {
        nm_vm_snapshot_to_db(name, &data);
    }

    done = 1;
    if (pthread_join(spin_th, NULL) != 0) {
        nm_bug(_("%s: cannot join thread"), __func__);
    }

out:
    NM_FORM_EXIT();
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_str_free(&data.snap_name);
    nm_str_free(&data.load);
}

static void nm_vm_snapshot_create_fields_setup(void)
{
    field_opts_off(fields[NM_FLD_VMSNAPNAME], O_STATIC);
    set_field_buffer(fields[NM_FLD_VMLOAD], 0, nm_form_yes_no[1]);
}

static size_t nm_vm_snapshot_create_labels_setup(void)
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_VMSNAPNAME:
            nm_str_format(&buf, "%s", _(NM_LC_VMSNAP_FORM_NAME));
            break;
        case NM_LBL_VMLOAD:
            nm_str_format(&buf, "%s", _(NM_LC_VMSNAP_FORM_LOAD));
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

void nm_vm_snapshot_delete(const nm_str_t *name, int vm_status)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t choices = NM_INIT_VECT;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    int done = 0;
    pthread_t spin_th;
    size_t snaps_count = 0;
    size_t msg_len;

    nm_str_format(&query, NM_GET_SNAPS_ALL_SQL, name->data);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0) {
        nm_warn(_(NM_MSG_NO_SNAPS));
        goto out;
    }

    snaps_count = snaps.n_memb / 5;
    for (size_t n = 0; n < snaps_count; n++) {
        size_t idx_shift = 5 * n;

        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_NAME + idx_shift),
            nm_vect_str_len(&snaps, NM_SQL_VMSNAP_NAME + idx_shift) + 1,
            NULL);
    }
    nm_vect_end_zero(&choices);

    nm_vm_snapshot_delete_init_windows(NULL);

    msg_len = mbstowcs(NULL, _(NM_LC_VMSNAP_FORM_SNAP),
            strlen(_(NM_LC_VMSNAP_FORM_SNAP)));

    form_data = nm_form_data_new(
        action_window, nm_vm_snapshot_delete_init_windows,
        msg_len, 1, NM_TRUE
    );

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    fields[0] = nm_field_label_new(0, form_data);
    fields[1] = nm_field_enum_new(0, form_data,
        (const char **)choices.data, false, false);
    fields[2] = NULL;

    set_field_buffer(fields[0], 0, _(NM_LC_VMSNAP_FORM_SNAP));
    set_field_buffer(fields[1], 0, *choices.data);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    nm_get_field_buf(fields[1], &buf);
    nm_form_check_data(_(NM_LC_VMSNAP_FORM_SNAP), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar,
                (void *) &sp_data) != 0) {
        nm_bug(_("%s: cannot create thread"), __func__);
    }

    __nm_vm_snapshot_delete(name, &buf, vm_status);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0) {
        nm_bug(_("%s: cannot join thread"), __func__);
    }

out:
    NM_FORM_EXIT();
    nm_vect_free(&snaps, nm_str_vect_free_cb);
    nm_vect_free(&choices, NULL);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_str_free(&query);
    nm_str_free(&buf);
}

void nm_vm_snapshot_load(const nm_str_t *name, int vm_status)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vect_t choices = NM_INIT_VECT;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    size_t snaps_count = 0;
    int done = 0;
    pthread_t spin_th;
    size_t msg_len;

    nm_str_format(&query, NM_GET_SNAPS_ALL_SQL, name->data);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0) {
        nm_warn(_(NM_MSG_NO_SNAPS));
        goto out;
    }

    snaps_count = snaps.n_memb / 5;
    for (size_t n = 0; n < snaps_count; n++) {
        size_t idx_shift = 5 * n;

        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_NAME + idx_shift),
            nm_vect_str_len(&snaps, NM_SQL_VMSNAP_NAME + idx_shift) + 1,
            NULL);
    }
    nm_vect_end_zero(&choices);

    nm_vm_snapshot_revert_init_windows(NULL);

    msg_len = mbstowcs(NULL, _(NM_LC_VMSNAP_FORM_SNAP),
            strlen(_(NM_LC_VMSNAP_FORM_SNAP)));

    form_data = nm_form_data_new(
        action_window, nm_vm_snapshot_delete_init_windows,
        msg_len, 1, NM_TRUE
    );

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    fields[0] = nm_field_label_new(0, form_data);
    fields[1] = nm_field_enum_new(0, form_data,
        (const char **)choices.data, false, false);
    fields[2] = NULL;

    set_field_buffer(fields[0], 0, _(NM_LC_VMSNAP_FORM_SNAP));
    set_field_buffer(fields[1], 0, *choices.data);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    nm_get_field_buf(fields[1], &buf);
    nm_form_check_data(_(NM_LC_VMSNAP_FORM_SNAP), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar,
                (void *) &sp_data) != 0) {
        nm_bug(_("%s: cannot create thread"), __func__);
    }

    __nm_vm_snapshot_load(name, &buf, vm_status);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0) {
        nm_bug(_("%s: cannot join thread"), __func__);
    }

out:
    NM_FORM_EXIT();
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_vect_free(&snaps, nm_str_vect_free_cb);
    nm_vect_free(&choices, NULL);
    nm_str_free(&query);
    nm_str_free(&buf);
}

static void __nm_vm_snapshot_load(const nm_str_t *name, const nm_str_t *snap,
                                  int vm_status)
{
    /* vm is not running, will load snapshot at next boot */
    if (!vm_status) {
        nm_str_t query = NM_INIT_STR;

        /* reset load flag for all snapshots for current vm */
        nm_str_format(&query, NM_RESET_LOAD_SQL, name->data);
        nm_db_edit(query.data);

        /* set load flag for current snapshot */
        nm_str_format(&query, NM_SNAP_UPDATE_LOAD_SQL, name->data, snap->data);
        nm_db_edit(query.data);

        nm_str_free(&query);
        return;
    }

    /* vm is running, load snapshot using QMP command loadvm */
    nm_qmp_loadvm(name, snap);
}

static void __nm_vm_snapshot_delete(const nm_str_t *name, const nm_str_t *snap,
                                    int vm_status)
{
    int rc;

    if (!vm_status) {
        /*
         * vm is not running, use
         * qemu-img snapshot -d snapshot_name path_to_drive system command
         */
        nm_str_t buf = NM_INIT_STR;
        nm_vect_t argv = NM_INIT_VECT;
        nm_str_t query = NM_INIT_STR;
        nm_vect_t drives = NM_INIT_VECT;

        /* get first drive name */
        nm_str_format(&query, NM_GET_BOOT_DRIVE_SQL, name->data);
        nm_db_select(query.data, &drives);
        if (drives.n_memb == 0) {
            nm_str_free(&query);
            return;
        }

        nm_str_format(&buf, "%s/qemu-img", nm_cfg_get()->qemu_bin_path.data);
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_vect_insert_cstr(&argv, "snapshot");
        nm_vect_insert_cstr(&argv, "-d");

        nm_vect_insert(&argv, snap->data, snap->len + 1, NULL);

        nm_str_format(&buf, "%s/%s/%s",
                nm_cfg_get()->vm_dir.data, name->data,
                nm_vect_str_ctx(&drives, 0));
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_vect_end_zero(&argv);
        if (nm_spawn_process(&argv, NULL) != NM_OK) {
            nm_bug(_("%s: cannot delete snapshot"), __func__);
        }

        rc = NM_OK;
        nm_str_free(&buf);
        nm_vect_free(&argv, NULL);
        nm_vect_free(&drives, nm_str_vect_free_cb);
        nm_str_free(&query);
    } else {
        /* vm is running, delete snapshot using QMP command delvm */
        rc = nm_qmp_delvm(name, snap);
    }


    if (rc == NM_OK) {
        /* delete snapshot from database */
        nm_str_t query = NM_INIT_STR;

        nm_str_format(&query, NM_DELETE_SNAP_SQL, name->data, snap->data);
        nm_db_edit(query.data);

        nm_str_free(&query);
    }
}

static int nm_vm_snapshot_get_data(const nm_str_t *name, nm_vmsnap_t *data)
{
    int rc;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t names = NM_INIT_VECT;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_VMSNAPNAME], &data->snap_name);
    nm_get_field_buf(fields[NM_FLD_VMLOAD], &data->load);

    nm_form_check_data(_(NM_LC_VMSNAP_FORM_NAME), data->snap_name, err);
    nm_form_check_data(_(NM_LC_VMSNAP_FORM_LOAD), data->load, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    nm_str_format(&query, NM_SNAP_GET_NAME_SQL,
            name->data, data->snap_name.data);
    nm_db_select(query.data, &names);

    if (names.n_memb != 0) {
        curs_set(0);
        nm_warn(_(NM_MSG_SNAP_OVER));
        rc = NM_ERR;
    }

out:
    nm_vect_free(&names, nm_str_vect_free_cb);
    nm_str_free(&query);
    return rc;
}

static void nm_vm_snapshot_to_db(const nm_str_t *name, const nm_vmsnap_t *data)
{
    nm_str_t query = NM_INIT_STR;
    int load = 0;

    if (nm_str_cmp_st(&data->load, "yes") == NM_OK) {
        load = 1;
    }

    if (!data->update) {
        nm_str_format(&query, NM_INSERT_SNAP_SQL,
            name->data, data->snap_name.data, load);
    } else {
        nm_str_format(&query, NM_UPDATE_SNAP_SQL, load,
            name->data, data->snap_name.data);
    }

    nm_db_edit(query.data);

    nm_str_free(&query);
}

#endif /* NM_OS_DARWIN */
/* vim:set ts=4 sw=4: */
