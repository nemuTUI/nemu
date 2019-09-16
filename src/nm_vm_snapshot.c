#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_qmp_control.h>
#include <nm_vm_snapshot.h>

static const char NM_FORMSTR_NAME[] = "Snapshot name";
static const char NM_FORMSTR_LOAD[] = "Load at next boot";
static const char NM_FORMSTR_SNAP[] = "Snapshot";

enum {
    NM_FLD_VMSNAPNAME = 0,
    NM_FLD_VMLOAD,
    NM_FLD_COUNT
};

static const char *nm_form_msg[] = {
    NM_FORMSTR_NAME, NM_FORMSTR_LOAD, NULL
};

enum {
    NM_SQL_VMSNAP_ID = 0,
    NM_SQL_VMSNAP_VM,
    NM_SQL_VMSNAP_NAME,
    NM_SQL_VMSNAP_LOAD,
    NM_SQL_VMSNAP_TIME
};

typedef struct {
    nm_str_t snap_name;
    nm_str_t load;
    int update;
} nm_vmsnap_t;

#define NM_INIT_VMSNAP (nm_vmsnap_t) { NM_INIT_STR, NM_INIT_STR, 0 }

static int nm_vm_snapshot_get_data(const nm_str_t *name, nm_vmsnap_t *data);
static void nm_vm_snapshot_to_db(const nm_str_t *name, const nm_vmsnap_t *data);
static void __nm_vm_snapshot_load(const nm_str_t *name, const nm_str_t *snap,
                                  int vm_status);
static void __nm_vm_snapshot_delete(const nm_str_t *name, const nm_str_t *snap,
                                    int vm_status);

static nm_field_t *fields[NM_FLD_COUNT + 1];

void nm_vm_snapshot_create(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_vmsnap_t data = NM_INIT_VMSNAP;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    pthread_t spin_th;
    int done = 0;
    size_t msg_len = nm_max_msg_len(nm_form_msg);

    if (nm_form_calc_size(msg_len, NM_FLD_COUNT, &form_data) != NM_OK)
        return;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_SNAP_CRT));
    nm_init_help_edit();

    for (size_t n = 0; n < NM_FLD_COUNT; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_FLD_COUNT] = NULL;
    field_opts_off(fields[NM_FLD_VMSNAPNAME], O_STATIC);
    set_field_type(fields[NM_FLD_VMLOAD], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_buffer(fields[NM_FLD_VMLOAD], 0, nm_form_yes_no[1]);

    for (size_t n = 0, y = 1, x = 2; n < NM_FLD_COUNT; n++)
    {
        mvwaddstr(form_data.form_window, y, x, _(nm_form_msg[n]));
        y += 2;
    }

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    if (nm_vm_snapshot_get_data(name, &data) != NM_OK)
        goto out;

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    if (nm_qmp_savevm(name, &data.snap_name) != NM_ERR)
        nm_vm_snapshot_to_db(name, &data);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    NM_FORM_EXIT();
    nm_form_free(form, fields);
    nm_str_free(&data.snap_name);
    nm_str_free(&data.load);
}

void nm_vm_snapshot_delete(const nm_str_t *name, int vm_status)
{
    nm_form_t *form = NULL;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t snaps = NM_INIT_VECT;
    nm_vect_t choices = NM_INIT_VECT;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    int done = 0;
    pthread_t spin_th;
    size_t snaps_count = 0;
    size_t msg_len = mbstowcs(NULL, _(NM_FORMSTR_SNAP), strlen(_(NM_FORMSTR_SNAP)));

    nm_str_format(&query, NM_GET_SNAPS_ALL_SQL, name->data);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0)
    {
        nm_warn(_(NM_MSG_NO_SNAPS));
        goto out;
    }

    if (nm_form_calc_size(msg_len, 1, &form_data) != NM_OK)
        goto out;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_SNAP_DEL));
    nm_init_help_edit();

    nm_print_snapshots(&snaps);

    snaps_count = snaps.n_memb / 5;
    for (size_t n = 0; n < snaps_count; n++)
    {
        size_t idx_shift = 5 * n;

        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_NAME + idx_shift),
            nm_vect_str_len(&snaps, NM_SQL_VMSNAP_NAME + idx_shift) + 1,
            NULL);
    }

    nm_vect_end_zero(&choices);

    fields[0] = new_field(1, form_data.form_len, 0, 0, 0, 0);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, choices.data, false, false);
    set_field_buffer(fields[0], 0, *choices.data);

    mvwaddstr(form_data.form_window, 1, 2, _(NM_FORMSTR_SNAP));

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    curs_set(0);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto clean_and_out;

    nm_get_field_buf(fields[0], &buf);
    nm_form_check_data(_(NM_FORMSTR_SNAP), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto clean_and_out;
    }

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    __nm_vm_snapshot_delete(name, &buf, vm_status);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

clean_and_out:
    werase(help_window);
    nm_init_help_main();

out:
    wtimeout(action_window, -1);
    delwin(form_data.form_window);
    nm_vect_free(&snaps, nm_str_vect_free_cb);
    nm_vect_free(&choices, NULL);
    nm_form_free(form, fields);
    nm_str_free(&query);
    nm_str_free(&buf);
}

void nm_vm_snapshot_load(const nm_str_t *name, int vm_status)
{
    nm_vect_t snaps = NM_INIT_VECT;
    nm_vect_t choices = NM_INIT_VECT;
    nm_vect_t err = NM_INIT_VECT;
    nm_form_t *snap_form = NULL;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    size_t snaps_count = 0;
    int done = 0;
    pthread_t spin_th;
    size_t msg_len = mbstowcs(NULL, NM_FORMSTR_SNAP, strlen(NM_FORMSTR_SNAP));

    nm_str_format(&query, NM_GET_SNAPS_ALL_SQL, name->data);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0)
    {
        nm_warn(_(NM_MSG_NO_SNAPS));
        goto out;
    }

    if (nm_form_calc_size(msg_len, 1, &form_data) != NM_OK)
        goto out;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_SNAP_REV));
    nm_init_help_edit();

    nm_print_snapshots(&snaps);

    snaps_count = snaps.n_memb / 5;
    for (size_t n = 0; n < snaps_count; n++)
    {
        size_t idx_shift = 5 * n;

        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_NAME + idx_shift),
            nm_vect_str_len(&snaps, NM_SQL_VMSNAP_NAME + idx_shift) + 1,
            NULL);
    }

    nm_vect_end_zero(&choices);

    fields[0] = new_field(1, form_data.form_len, 0, 0, 0, 0);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, choices.data, false, false);
    set_field_buffer(fields[0], 0, *choices.data);

    mvwaddstr(form_data.form_window, 1, 2, _(NM_FORMSTR_SNAP));

    snap_form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    curs_set(0);
    if (nm_draw_form(action_window, snap_form) != NM_OK)
        goto clean_and_out;

    nm_get_field_buf(fields[0], &buf);
    nm_form_check_data(_(NM_FORMSTR_SNAP), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto clean_and_out;
    }

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    __nm_vm_snapshot_load(name, &buf, vm_status);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

clean_and_out:
    werase(help_window);
    nm_init_help_main();

out:
    wtimeout(action_window, -1);
    delwin(form_data.form_window);
    nm_form_free(snap_form, fields);
    nm_vect_free(&snaps, nm_str_vect_free_cb);
    nm_vect_free(&choices, NULL);
    nm_str_free(&query);
    nm_str_free(&buf);
}

static void __nm_vm_snapshot_load(const nm_str_t *name, const nm_str_t *snap,
                                  int vm_status)
{
    /* vm is not running, will load snapshot at next boot */
    if (!vm_status)
    {
        nm_str_t query = NM_INIT_STR;

        /* reset load flag for all snapshots for current vm */
        nm_str_format(&query, NM_RESET_LOAD_SQL, name->data);
        nm_db_edit(query.data);

        /* set load flag for current shapshot */
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
    int rc = NM_ERR;

    if (!vm_status)
    {
        /* vm is not running, use
         * qemu-img snapshot -d snapshot_name path_to_drive system command */
        nm_str_t buf = NM_INIT_STR;
        nm_vect_t argv = NM_INIT_VECT;
        nm_str_t query = NM_INIT_STR;
        nm_vect_t drives = NM_INIT_VECT;

        /* get first drive name */
        nm_str_format(&query, NM_GET_BOOT_DRIVE_SQL, name->data);
        nm_db_select(query.data, &drives);
        assert(drives.n_memb != 0);

        nm_str_alloc_text(&buf, NM_STRING(NM_USR_PREFIX) "/bin/qemu-img");
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_vect_insert_cstr(&argv, "snapshot");
        nm_vect_insert_cstr(&argv, "-d");

        nm_vect_insert(&argv, snap->data, snap->len + 1, NULL);

        nm_str_format(&buf, "%s/%s/%s", nm_cfg_get()->vm_dir.data, name->data, nm_vect_str_ctx(&drives, 0));
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_vect_end_zero(&argv);
        if (nm_spawn_process(&argv, NULL) != NM_OK)
            nm_bug(_("%s: cannot delete snapshot"), __func__);

        rc = NM_OK;
        nm_str_free(&buf);
        nm_vect_free(&argv, NULL);
        nm_vect_free(&drives, nm_str_vect_free_cb);
        nm_str_free(&query);
    }
    else
    {
        /* vm is running, delete snapshot using QMP command delvm */
        rc = nm_qmp_delvm(name, snap);
    }


    if (rc == NM_OK)
    {
        /* delete snapshot from database */
        nm_str_t query = NM_INIT_STR;

        nm_str_format(&query, NM_DELETE_SNAP_SQL, name->data, snap->data);
        nm_db_edit(query.data);

        nm_str_free(&query);
    }
}

static int nm_vm_snapshot_get_data(const nm_str_t *name, nm_vmsnap_t *data)
{
    int rc = NM_OK;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t names = NM_INIT_VECT;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_VMSNAPNAME], &data->snap_name);
    nm_get_field_buf(fields[NM_FLD_VMLOAD], &data->load);

    nm_form_check_data(_(NM_FORMSTR_NAME), data->snap_name, err);
    nm_form_check_data(_(NM_FORMSTR_LOAD), data->load, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    nm_str_format(&query, NM_SNAP_GET_NAME_SQL, name->data, data->snap_name.data);
    nm_db_select(query.data, &names);

    if (names.n_memb != 0)
    {
        curs_set(0);
        int ch = nm_notify(_(NM_MSG_SNAP_OVER));

        if (ch == 'y')
            data->update = 1;
        else
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

    if (nm_str_cmp_st(&data->load, "yes") == NM_OK)
        load = 1;

    if (!data->update)
    {
        nm_str_format(&query, NM_INSERT_SNAP_SQL,
            name->data, data->snap_name.data, load);
    }
    else
    {
        nm_str_format(&query, NM_UPDATE_SNAP_SQL, load,
            name->data, data->snap_name.data);
    }

    nm_db_edit(query.data);

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
