#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>

typedef struct {
#if defined(NM_WITH_SPICE)
    nm_str_t spice;
#endif
    nm_str_t port;
    nm_str_t tty;
    nm_str_t sock;
    nm_str_t sync;
    nm_str_t display;
} nm_view_data_t;

#if defined(NM_WITH_SPICE)
#define NM_INIT_VIEW_DATA (nm_view_data_t) { NM_INIT_STR, NM_INIT_STR, \
        NM_INIT_STR,  NM_INIT_STR, NM_INIT_STR, NM_INIT_STR }
#else
#define NM_INIT_VIEW_DATA (nm_view_data_t) { NM_INIT_STR, NM_INIT_STR, \
    NM_INIT_STR, NM_INIT_STR }
#endif

static inline void nm_viewer_free(nm_view_data_t *data)
{
#if defined(NM_WITH_SPICE)
    nm_str_free(&data->spice);
#endif
    nm_str_free(&data->port);
    nm_str_free(&data->tty);
    nm_str_free(&data->sock);
    nm_str_free(&data->sync);
    nm_str_free(&data->display);
}

static int nm_viewer_get_data(nm_view_data_t *vm);
static void nm_viewer_update_db(const nm_str_t *name, nm_view_data_t *vm);

enum {
#if defined(NM_WITH_SPICE)
    NM_FLD_SPICE = 0,
    NM_FLD_PORT,
#else
    NM_FLD_PORT = 0,
#endif
    NM_FLD_TTYP,
    NM_FLD_SOCK,
    NM_FLD_SYNC,
    NM_FLD_DSP,
    NM_FLD_COUNT,
};

static const char *nm_form_msg[] = {
#if defined(NM_WITH_SPICE)
    "Spice server",
    "VNC/Spice port",
#else
    "VNC port",
#endif
    "Serial TTY", "Serial socket",
    "Sync mouse position",
    "Display type", NULL
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

void nm_viewer(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    nm_view_data_t vm_new = NM_INIT_VIEW_DATA;
    uint32_t vnc_port;
    size_t msg_len = nm_max_msg_len(nm_form_msg);

    if (nm_form_calc_size(msg_len, NM_FLD_COUNT, &form_data) != NM_OK)
        return;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_EDIT_VIEW));
    nm_init_help_edit();

    nm_vmctl_get_data(name, &vm);

    for (size_t n = 0; n < NM_FLD_COUNT; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_FLD_COUNT] = NULL;
#if defined(NM_WITH_SPICE)
    set_field_type(fields[NM_FLD_SPICE], TYPE_ENUM, nm_form_yes_no, false, false);
    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_SPICE), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_SPICE], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_SPICE], 0, nm_form_yes_no[1]);
#endif
    set_field_type(fields[NM_FLD_PORT], TYPE_INTEGER, 1, NM_STARTING_VNC_PORT, 0xffff);
    set_field_type(fields[NM_FLD_TTYP], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_SOCK], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_SYNC], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_DSP], TYPE_ENUM, nm_form_displaytype, false, false);

    field_opts_off(fields[NM_FLD_TTYP], O_STATIC);
    field_opts_off(fields[NM_FLD_SOCK], O_STATIC);


    vnc_port = nm_str_stoui(nm_vect_str(&vm.main, NM_SQL_VNC), 10) + NM_STARTING_VNC_PORT;
    nm_str_format(nm_vect_str(&vm.main, NM_SQL_VNC), "%u", vnc_port);
    set_field_buffer(fields[NM_FLD_PORT], 0, nm_vect_str_ctx(&vm.main, NM_SQL_VNC));
    set_field_buffer(fields[NM_FLD_TTYP], 0, nm_vect_str_ctx(&vm.main, NM_SQL_TTY));
    set_field_buffer(fields[NM_FLD_SOCK], 0, nm_vect_str_ctx(&vm.main, NM_SQL_SOCK));
    set_field_buffer(fields[NM_FLD_DSP], 0, nm_vect_str_ctx(&vm.main, NM_SQL_DISPLAY));

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_OVER), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_SYNC], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_SYNC], 0, nm_form_yes_no[1]);

    for (size_t n = 0; n < NM_FLD_COUNT; n++)
        set_field_status(fields[n], 0);

    for (size_t n = 0, y = 1, x = 2; n < NM_FLD_COUNT; n++) {
        mvwaddstr(form_data.form_window, y, x, _(nm_form_msg[n]));
        y += 2;
    }

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    if (nm_viewer_get_data(&vm_new) != NM_OK)
        goto out;

    nm_viewer_update_db(name, &vm_new);

out:
    NM_FORM_EXIT();
    nm_viewer_free(&vm_new);
    nm_vmctl_free_data(&vm);
    nm_form_free(form, fields);
}

static int nm_viewer_get_data(nm_view_data_t *vm)
{
    int rc;
    nm_vect_t err = NM_INIT_VECT;

#if defined(NM_WITH_SPICE)
    nm_get_field_buf(fields[NM_FLD_SPICE], &vm->spice);
#endif
    nm_get_field_buf(fields[NM_FLD_PORT], &vm->port);
    nm_get_field_buf(fields[NM_FLD_TTYP], &vm->tty);
    nm_get_field_buf(fields[NM_FLD_SOCK], &vm->sock);
    nm_get_field_buf(fields[NM_FLD_SYNC], &vm->sync);
    nm_get_field_buf(fields[NM_FLD_DSP],  &vm->display);

#if defined(NM_WITH_SPICE)
    if (field_status(fields[NM_FLD_SPICE]))
        nm_form_check_data(nm_form_msg[NM_FLD_SPICE], vm->spice, err);
#endif
    if (field_status(fields[NM_FLD_PORT]))
        nm_form_check_data(nm_form_msg[NM_FLD_PORT], vm->port, err);
    if (field_status(fields[NM_FLD_SYNC]))
        nm_form_check_data(nm_form_msg[NM_FLD_SYNC], vm->sync, err);
    if (field_status(fields[NM_FLD_SYNC]))
        nm_form_check_data(nm_form_msg[NM_FLD_SYNC], vm->sync, err);
    if (field_status(fields[NM_FLD_DSP]))
        nm_form_check_data(nm_form_msg[NM_FLD_DSP], vm->display, err);

    rc = nm_print_empty_fields(&err);

    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_viewer_update_db(const nm_str_t *name, nm_view_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;

#if defined(NM_WITH_SPICE)
    if (field_status(fields[NM_FLD_SPICE])) {
        int spice_on = 0;

        if (nm_str_cmp_st(&vm->spice, "yes") == NM_OK)
            spice_on = 1;
        nm_str_format(&query, "UPDATE vms SET spice='%s' WHERE name='%s'",
            spice_on ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }
#endif /* NM_WITH_SPICE */

    if (field_status(fields[NM_FLD_PORT])) {
        uint32_t vnc_port = nm_str_stoui(&vm->port, 10) - NM_STARTING_VNC_PORT;
        nm_str_format(&query, "UPDATE vms SET vnc='%u' WHERE name='%s'",
                vnc_port, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_TTYP])) {
        nm_str_format(&query, "UPDATE vms SET tty_path='%s' WHERE name='%s'",
            vm->tty.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SOCK])) {
        nm_str_format(&query, "UPDATE vms SET socket_path='%s' WHERE name='%s'",
            vm->sock.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SYNC])) {
        int sync_on = 0;

        if (nm_str_cmp_st(&vm->sync, "yes") == NM_OK)
            sync_on = 1;
        nm_str_format(&query, "UPDATE vms SET mouse_override='%s' WHERE name='%s'",
            sync_on ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DSP])) {
        nm_str_format(&query, "UPDATE vms SET display_type='%s' WHERE name='%s'",
            vm->display.data, name->data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}
/* vim:set ts=4 sw=4: */
