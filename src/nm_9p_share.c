#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

#define NM_9P_FIELDS_NUM 3

#define NM_INIT_9P_DATA { NM_INIT_STR, NM_INIT_STR, NM_INIT_STR }
#define NM_9P_SET_MODE_SQL \
    "UPDATE vms SET fs9p_enable='%s' WHERE name='%s'"
#define NM_9P_SET_PATH_SQL \
    "UPDATE vms SET fs9p_path='%s' WHERE name='%s'"
#define NM_9P_SET_NAME_SQL \
    "UPDATE vms SET fs9p_name='%s' WHERE name='%s'"

typedef struct {
    nm_str_t mode;
    nm_str_t path;
    nm_str_t name;
} nm_9p_data_t;

enum {
    NM_FLD_9PMODE = 0,
    NM_FLD_9PPATH,
    NM_FLD_9PNAME
};

static nm_field_t *fields[NM_9P_FIELDS_NUM + 1];

static int nm_9p_get_data(nm_9p_data_t *data, const nm_vmctl_data_t *cur);
static void nm_9p_update_db(const nm_str_t *name, const nm_9p_data_t *data);

void nm_9p_share(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    nm_9p_data_t data = NM_INIT_9P_DATA;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_vmctl_get_data(name, &vm);

    nm_print_title(_(NM_EDIT_TITLE));
    //window = nm_init_window(11, 51, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_9P_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 25, (n + 1) * 2, 1, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_9P_FIELDS_NUM] = NULL;

    set_field_type(fields[NM_FLD_9PMODE], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_9PPATH], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_9PNAME], TYPE_REGEXP, ".*");
    field_opts_off(fields[NM_FLD_9PPATH], O_STATIC);
    field_opts_off(fields[NM_FLD_9PNAME], O_STATIC);

    set_field_buffer(fields[NM_FLD_9PMODE], 0,
        (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_9FLG),
            NM_ENABLE) == NM_OK) ? "yes" : "no");
    set_field_buffer(fields[NM_FLD_9PPATH], 0, nm_vect_str_ctx(&vm.main, NM_SQL_9PTH));
    set_field_buffer(fields[NM_FLD_9PNAME], 0, nm_vect_str_ctx(&vm.main, NM_SQL_9ID));

    nm_str_alloc_text(&buf, _("Share files to "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Enable sharing"));
    mvwaddstr(window, 6, 2, _("Path to directory"));
    mvwaddstr(window, 8, 2, _("Name of the share"));

    form = nm_post_form(window, fields, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_9p_get_data(&data, &vm) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_9p_update_db(name, &data);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_str_free(&data.mode);
    nm_str_free(&data.name);
    nm_str_free(&data.path);
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
    nm_str_free(&buf);
}

static int nm_9p_get_data(nm_9p_data_t *data, const nm_vmctl_data_t *cur)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_9PMODE], &data->mode);
    nm_get_field_buf(fields[NM_FLD_9PPATH], &data->path);
    nm_get_field_buf(fields[NM_FLD_9PNAME], &data->name);

    if (field_status(fields[NM_FLD_9PMODE]))
    {
        if (nm_str_cmp_st(&data->mode, "no") == NM_OK)
            goto out;
    }
    else
    {
        if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_9FLG), NM_ENABLE) != NM_OK)
            goto out;
    }

    nm_form_check_data(_("Path to directory"), data->path, err);
    nm_form_check_data(_("Name of the share"), data->name, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        nm_vect_free(&err, NULL);

out:
    return rc;
}

static void nm_9p_update_db(const nm_str_t *name, const nm_9p_data_t *data)
{
    nm_str_t query = NM_INIT_STR;
    nm_str_free(&query);

    if (field_status(fields[NM_FLD_9PMODE]))
    {
        nm_str_format(&query, NM_9P_SET_MODE_SQL,
            (nm_str_cmp_st(&data->mode, "yes") == NM_OK) ? "1" : "0", name->data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_9PPATH]))
    {
        nm_str_format(&query, NM_9P_SET_PATH_SQL, data->path.data, name->data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_9PNAME]))
    {
        nm_str_format(&query, NM_9P_SET_NAME_SQL, data->name.data, name->data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
