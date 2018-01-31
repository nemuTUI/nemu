#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_qmp_control.h>

#define NM_DEL_SNAP_TITLE NM_EDIT_TITLE " [delete]"

#define NM_VMSNAP_FIELDS_NUM 2
#define NM_FORMSTR_NAME "Snapshot name"
#define NM_FORMSTR_LOAD "Load at next boot"

#define NM_GET_NAME_SQL \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s' " \
    "AND snap_name='%s'"
#define NM_GET_SNAPS_ALL_SQL \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s' " \
    "ORDER BY timestamp ASC"
#define NM_GET_SNAPS_NAME_SQL \
    "SELECT snap_name FROM vmsnapshots WHERE vm_name='%s' " \
    "ORDER BY timestamp ASC"
#define NM_UPDATE_LOAD_SQL \
    "UPDATE vmsnapshots SET load='1' " \
    "WHERE vm_name='%s' AND snap_name='%s'"
#define NM_DELETE_SNAP_SQL \
    "DELETE FROM vmsnapshots WHERE vm_name='%s' " \
    "AND snap_name='%s'"
#define NM_UPDATE_SNAP_SQL \
    "UPDATE vmsnapshots SET load='%d', " \
    "timestamp=DATETIME('now','localtime') " \
    "WHERE vm_name='%s' AND snap_name='%s'"
#define NM_CHECK_SNAP_SQL \
    "SELECT id FROM snapshots WHERE vm_name='%s'"

enum {
    NM_FLD_VMSNAPNAME = 0,
    NM_FLD_VMLOAD,
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

#define NM_INIT_VMSNAP { NM_INIT_STR, NM_INIT_STR, 0 }

static int nm_vm_snapshot_get_data(const nm_str_t *name, nm_vmsnap_t *data);
static void nm_vm_snapshot_to_db(const nm_str_t *name, const nm_vmsnap_t *data);
static void __nm_vm_snapshot_load(const nm_str_t *name, const nm_str_t *snap,
                                  int vm_status);
static void __nm_vm_snapshot_delete(const nm_str_t *name, const nm_str_t *snap,
                                    int vm_status);

static nm_field_t *fields[NM_VMSNAP_FIELDS_NUM + 1];

void nm_vm_snapshot_create(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vmsnap_t data = NM_INIT_VMSNAP;
    nm_vect_t drive_snaps = NM_INIT_VECT;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_str_format(&query, NM_CHECK_SNAP_SQL, name->data);
    nm_db_select(query.data, &drive_snaps);

    if (drive_snaps.n_memb)
    {
        nm_print_warn(3, 6, _("There should not be snapshots of drives"));
        goto out;
    }

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(9, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_VMSNAP_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 19, (n + 1) * 2, 1, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_VMSNAP_FIELDS_NUM] = NULL;
    field_opts_off(fields[NM_FLD_VMSNAPNAME], O_STATIC);
    set_field_type(fields[NM_FLD_VMLOAD], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_buffer(fields[NM_FLD_VMLOAD], 0, nm_form_yes_no[1]);

    nm_str_alloc_text(&buf, _("Snapshot "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _(NM_FORMSTR_NAME));
    mvwaddstr(window, 6, 2, _(NM_FORMSTR_LOAD));

    form = nm_post_form(window, fields, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_vm_snapshot_get_data(name, &data) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    if (nm_qmp_savevm(name, &data.snap_name) != NM_ERR)
        nm_vm_snapshot_to_db(name, &data);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vect_free(&drive_snaps, nm_str_vect_free_cb);
    nm_form_free(form, fields);
    nm_str_free(&data.snap_name);
    nm_str_free(&data.load);
    nm_str_free(&query);
    nm_str_free(&buf);
}

void nm_vm_snapshot_delete(const nm_str_t *name, int vm_status)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t snaps = NM_INIT_VECT;
    nm_vect_t choices = NM_INIT_VECT;
    size_t msg_len;
    int done = 0;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    pthread_t spin_th;

    nm_str_format(&query, NM_GET_SNAPS_NAME_SQL, name->data);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0)
    {
        nm_print_warn(3, 6, _("There are no snapshots"));
        goto out;
    }

    nm_print_title(_(NM_DEL_SNAP_TITLE));

    for (size_t n = 0; n < snaps.n_memb; n++)
    {
        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, n),
            nm_vect_str_len(&snaps, n) + 1,
            NULL);
    }

    nm_vect_end_zero(&choices);

    window = nm_init_window(7, 45, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    fields[0] = new_field(1, 30, 2, 1, 0, 0);
    set_field_back(fields[0], A_UNDERLINE);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, choices.data, false, false);
    set_field_buffer(fields[0], 0, *choices.data);

    nm_str_add_text(&buf, _("Snapshots of "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Snapshot"));
    nm_str_trunc(&buf, 0);

    form = nm_post_form(window, fields, 11);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[0], &buf);
    nm_form_check_data(_("Snapshot"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    msg_len = mbstowcs(NULL, _(NM_DEL_SNAP_TITLE), strlen(_(NM_DEL_SNAP_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    __nm_vm_snapshot_delete(name, &buf, vm_status);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
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
    nm_window_t *revert_window = NULL;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    size_t snaps_count = 0, msg_len;
    int pos_y = 11, pos_x = 2, done = 0;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    pthread_t spin_th;

    nm_str_format(&query, NM_GET_SNAPS_ALL_SQL, name->data);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0)
    {
        nm_print_warn(3, 6, _("There are no snapshots"));
        goto out;
    }

    nm_print_title(_(NM_EDIT_TITLE));

    snaps_count = snaps.n_memb / 5;
    for (size_t n = 0; n < snaps_count; n++)
    {
        size_t idx_shift = 5 * n;

        if (pos_x >= (getmaxx(stdscr) - 20))
        {
            pos_y++;
            pos_x = 2;
        }

        mvprintw(pos_y, pos_x, nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_NAME + idx_shift));
        pos_x += nm_vect_str_len(&snaps, NM_SQL_VMSNAP_NAME + idx_shift);
        mvprintw(pos_y, pos_x, "(");
        pos_x++;
        mvprintw(pos_y, pos_x, nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_TIME + idx_shift));
        pos_x += nm_vect_str_len(&snaps, NM_SQL_VMSNAP_TIME + idx_shift);
        mvprintw(pos_y, pos_x, ") -> ");
        pos_x += 5;

        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, NM_SQL_VMSNAP_NAME + idx_shift),
            nm_vect_str_len(&snaps, NM_SQL_VMSNAP_NAME + idx_shift) + 1,
            NULL);
    }

    mvprintw(pos_y, pos_x, "current");

    nm_vect_end_zero(&choices);

    revert_window = nm_init_window(7, 45, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(revert_window, COLOR_PAIR(1));

    fields[0] = new_field(1, 30, 2, 1, 0, 0);
    set_field_back(fields[0], A_UNDERLINE);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, choices.data, false, false);
    set_field_buffer(fields[0], 0, *choices.data);

    nm_str_add_text(&buf, _("Revert "));
    nm_str_add_str(&buf, name);
    mvwaddstr(revert_window, 1, 2, buf.data);
    mvwaddstr(revert_window, 4, 2, _("Snapshot"));
    nm_str_trunc(&buf, 0);

    snap_form = nm_post_form(revert_window, fields, 11);
    if (nm_draw_form(revert_window, snap_form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[0], &buf);
    nm_form_check_data(_("Snapshot"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    __nm_vm_snapshot_load(name, &buf, vm_status);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
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
        nm_str_trunc(&query, 0);

        /* set load flag for current shapshot */
        nm_str_format(&query, NM_UPDATE_LOAD_SQL, name->data, snap->data);
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
        nm_str_t cmd = NM_INIT_STR;

        nm_str_alloc_text(&cmd, NM_STRING(NM_USR_PREFIX) "/bin/qemu-img snapshot -d ");
        nm_str_add_str(&cmd, snap);
        nm_str_add_char(&cmd, ' ');
        nm_str_add_str(&cmd, &nm_cfg_get()->vm_dir);
        nm_str_format(&cmd, "/%s/%s_a.img", name->data, name->data);

        if (nm_spawn_process(&cmd) != NM_OK)
            nm_bug(_("%s: cannot delete snapshot"), __func__);

        rc = NM_OK;
        nm_str_free(&cmd);
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

    nm_str_format(&query, NM_GET_NAME_SQL, name->data, data->snap_name.data);
    nm_db_select(query.data, &names);

    if (names.n_memb != 0)
    {
        int ch = nm_print_warn(3, 2, _("Override snapshot? (y/n)"));

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
        nm_str_alloc_text(&query, "INSERT INTO vmsnapshots("
            "vm_name, snap_name, load, timestamp) VALUES('");
        nm_str_format(&query, "%s', '%s', '%d', DATETIME('now','localtime'))",
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
