#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_9p_share.h>

static const char NM_9P_SET_MODE_SQL[] =
    "UPDATE vms SET fs9p_enable='%s' WHERE name='%s'";
static const char NM_9P_SET_PATH_SQL[] =
    "UPDATE vms SET fs9p_path='%s' WHERE name='%s'";
static const char NM_9P_SET_NAME_SQL[] =
    "UPDATE vms SET fs9p_name='%s' WHERE name='%s'";

typedef struct {
    nm_str_t mode;
    nm_str_t path;
    nm_str_t name;
} nm_9p_data_t;

#define NM_INIT_9P_DATA (nm_9p_data_t) { NM_INIT_STR, NM_INIT_STR, NM_INIT_STR }

enum {
    NM_FLD_9PMODE = 0,
    NM_FLD_9PPATH,
    NM_FLD_9PNAME,
    NM_FLD_COUNT
};

static const char *nm_form_msg[] = {
    "Enable sharing", "Path to directory",
    "Name of the share", NULL
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static int nm_9p_get_data(nm_9p_data_t *data, const nm_vmctl_data_t *cur);
static void nm_9p_update_db(const nm_str_t *name, const nm_9p_data_t *data);

void nm_9p_share(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_9p_data_t data = NM_INIT_9P_DATA;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    size_t msg_len = nm_max_msg_len(nm_form_msg);

    if (nm_form_calc_size(msg_len, NM_FLD_COUNT, &form_data) != NM_OK)
        return;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_9P_SHARE));
    nm_init_help_edit();

    nm_vmctl_get_data(name, &vm);

    for (size_t n = 0; n < NM_FLD_COUNT; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_FLD_COUNT] = NULL;

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

    for (size_t n = 0, y = 1, x = 2; n < NM_FLD_COUNT; n++)
    {
        mvwaddstr(form_data.form_window, y, x, _(nm_form_msg[n]));
        y += 2;
    }

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    if (nm_9p_get_data(&data, &vm) != NM_OK)
        goto out;

    nm_9p_update_db(name, &data);

out:
    NM_FORM_EXIT();
    nm_str_free(&data.mode);
    nm_str_free(&data.name);
    nm_str_free(&data.path);
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
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

    nm_form_check_data(_(nm_form_msg[1]), data->path, err);
    nm_form_check_data(_(nm_form_msg[2]), data->name, err);

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

/* vim:set ts=4 sw=4: */
