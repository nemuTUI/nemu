#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_qmp_control.h>

#define NM_VMSNAP_FIELDS_NUM 2
#define NM_FORMSTR_NAME "Snapshot name"
#define NM_FORMSTR_LOAD "Load at next boot"
#define NM_GET_NAME_SQL \
    "SELECT * FROM vmsnapshots WHERE vm_name='%s'" \
    " AND snap_name='%s'"

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
} nm_vmsnap_t;

#define NM_INIT_VMSNAP { NM_INIT_STR, NM_INIT_STR }

static int nm_vm_snapshot_get_data(const nm_str_t *name, nm_vmsnap_t *data);
static void nm_vm_snapshot_to_db(const nm_str_t *name, const nm_vmsnap_t *data);

static nm_field_t *fields[NM_VMSNAP_FIELDS_NUM + 1];

void nm_vm_snapshot_create(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    nm_vmsnap_t data = NM_INIT_VMSNAP;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

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
    nm_form_free(form, fields);
    nm_str_free(&data.snap_name);
    nm_str_free(&data.load);
    nm_str_free(&buf);
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
        nm_print_warn(3, 6, _("This name is already taken"));
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

    nm_str_alloc_text(&query, "INSERT INTO vmsnapshots("
        "vm_name, snap_name, load) VALUES('");
    nm_str_format(&query, "%s', '%s', '%d')",
        name->data, data->snap_name.data, load);

    nm_db_edit(query.data);

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
