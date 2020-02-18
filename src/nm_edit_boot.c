#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_machine.h>
#include <nm_database.h>
#include <nm_vm_control.h>
#include <nm_edit_boot.h>

enum {
    NM_FLD_INST = 0,
    NM_FLD_SRCP,
    NM_FLD_MACH,
    NM_FLD_BIOS,
    NM_FLD_KERN,
    NM_FLD_CMDL,
    NM_FLD_INIT,
    NM_FLD_TTYP,
    NM_FLD_SOCK,
    NM_FLD_DEBP,
    NM_FLD_DEBF,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static const char *nm_form_msg[] = {
    "OS Installed",   "Path to ISO/IMG",    "Machine type",
    "Path to BIOS",   "Path to kernel",     "Kernel cmdline",
    "Path to initrd", "Serial TTY",         "Serial socket",
    "GDB debug port", "Freeze after start",
    NULL
};

static void nm_edit_boot_field_setup(const nm_vmctl_data_t *cur);
static void nm_edit_boot_field_names(nm_window_t *w);
static int nm_edit_boot_get_data(nm_vm_boot_t *vm);
static void nm_edit_boot_update_db(const nm_str_t *name, nm_vm_boot_t *vm);

void nm_edit_boot(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_vm_boot_t vm = NM_INIT_VM_BOOT;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    nm_vmctl_data_t cur_settings = NM_VMCTL_INIT_DATA;
    size_t msg_len = nm_max_msg_len(nm_form_msg);

    if (nm_form_calc_size(msg_len, NM_FLD_COUNT, &form_data) != NM_OK)
        return;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_EDIT_BOOT));
    nm_init_help_edit();

    nm_vmctl_get_data(name, &cur_settings);

    for (size_t n = 0; n < NM_FLD_COUNT; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_FLD_COUNT] = NULL;

    nm_edit_boot_field_setup(&cur_settings);
    nm_edit_boot_field_names(form_data.form_window);

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    if (nm_edit_boot_get_data(&vm) != NM_OK)
        goto out;

    nm_edit_boot_update_db(name, &vm);

out:
    NM_FORM_EXIT();
    nm_vmctl_free_data(&cur_settings);
    nm_vm_free_boot(&vm);
    nm_form_free(form, fields);
}

static void nm_edit_boot_field_setup(const nm_vmctl_data_t *cur)
{
    const char **machs = NULL;

    machs = nm_mach_get(nm_vect_str(&cur->main, NM_SQL_ARCH));

    for (size_t n = 1; n < NM_FLD_COUNT; n++)
        field_opts_off(fields[n], O_STATIC);

    set_field_type(fields[NM_FLD_INST], TYPE_ENUM, nm_form_yes_no, false,
                   false);
    set_field_type(fields[NM_FLD_SRCP], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_MACH], TYPE_ENUM, machs, false, false);
    set_field_type(fields[NM_FLD_BIOS], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_KERN], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_CMDL], TYPE_REGEXP, ".*");
    set_field_type(fields[NM_FLD_INIT], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_TTYP], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_SOCK], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_DEBP], TYPE_INTEGER, 1, 0, 65535);
    set_field_type(fields[NM_FLD_DEBF], TYPE_ENUM, nm_form_yes_no, false,
                   false);

    if (machs == NULL)
        field_opts_off(fields[NM_FLD_MACH], O_ACTIVE);

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_INST), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_INST], 0, nm_form_yes_no[1]);
    else
        set_field_buffer(fields[NM_FLD_INST], 0, nm_form_yes_no[0]);

    set_field_buffer(fields[NM_FLD_MACH], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_MACH));
    set_field_buffer(fields[NM_FLD_SRCP], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_ISO));
    set_field_buffer(fields[NM_FLD_BIOS], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_BIOS));
    set_field_buffer(fields[NM_FLD_KERN], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_KERN));
    set_field_buffer(fields[NM_FLD_CMDL], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_KAPP));
    set_field_buffer(fields[NM_FLD_INIT], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_INIT));
    set_field_buffer(fields[NM_FLD_TTYP], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_TTY));
    set_field_buffer(fields[NM_FLD_SOCK], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_SOCK));
    set_field_buffer(fields[NM_FLD_DEBP], 0,
                     nm_vect_str_ctx(&cur->main, NM_SQL_DEBP));
    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_DEBF), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_DEBF], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_DEBF], 0, nm_form_yes_no[1]);

    for (size_t n = 0; n < NM_FLD_COUNT; n++)
        set_field_status(fields[n], 0);
}

static void nm_edit_boot_field_names(nm_window_t *w)
{
    int y = 1, x = 2, mult = 2;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        mvwaddstr(w, y, x, _(nm_form_msg[n]));
        y += mult;
    }
}

static int nm_edit_boot_get_data(nm_vm_boot_t *vm)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t inst = NM_INIT_STR;
    nm_str_t debug_freeze = NM_INIT_STR;

    nm_get_field_buf(fields[NM_FLD_INST], &inst);
    nm_get_field_buf(fields[NM_FLD_MACH], &vm->mach);
    nm_get_field_buf(fields[NM_FLD_SRCP], &vm->inst_path);
    nm_get_field_buf(fields[NM_FLD_BIOS], &vm->bios);
    nm_get_field_buf(fields[NM_FLD_KERN], &vm->kernel);
    nm_get_field_buf(fields[NM_FLD_CMDL], &vm->cmdline);
    nm_get_field_buf(fields[NM_FLD_INIT], &vm->initrd);
    nm_get_field_buf(fields[NM_FLD_TTYP], &vm->tty);
    nm_get_field_buf(fields[NM_FLD_SOCK], &vm->socket);
    nm_get_field_buf(fields[NM_FLD_DEBP], &vm->debug_port);
    nm_get_field_buf(fields[NM_FLD_DEBF], &debug_freeze);

    if (field_status(fields[NM_FLD_INST]))
        nm_form_check_data(_("OS Installed"), inst, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        goto out;

    if (nm_str_cmp_st(&inst, "no") == NM_OK) {
        vm->installed = 1;

        if (vm->inst_path.len == 0) {
            nm_warn(_(NM_MSG_ISO_MISS));
            rc = NM_ERR;
            goto out;
        }
    }

    if (nm_str_cmp_st(&debug_freeze, "yes") == NM_OK)
        vm->debug_freeze = 1;
    else
        vm->debug_freeze = 0;

out:
    nm_str_free(&inst);
    nm_str_free(&debug_freeze);
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_edit_boot_update_db(const nm_str_t *name, nm_vm_boot_t *vm)
{
    nm_str_t query = NM_INIT_STR;

    if (field_status(fields[NM_FLD_INST])) {
        nm_str_format(&query, "UPDATE vms SET install='%s' WHERE name='%s'",
                      vm->installed ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_MACH])) {
        nm_str_format(&query, "UPDATE vms SET machine='%s' WHERE name='%s'",
                      vm->mach.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SRCP])) {
        nm_str_format(&query, "UPDATE vms SET iso='%s' WHERE name='%s'",
                      vm->inst_path.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_BIOS])) {
        nm_str_format(&query, "UPDATE vms SET bios='%s' WHERE name='%s'",
                      vm->bios.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_KERN])) {
        nm_str_format(&query, "UPDATE vms SET kernel='%s' WHERE name='%s'",
                      vm->kernel.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_CMDL])) {
        nm_str_format(&query,
                      "UPDATE vms SET kernel_append='%s' WHERE name='%s'",
                      vm->cmdline.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_INIT])) {
        nm_str_format(&query, "UPDATE vms SET initrd='%s' WHERE name='%s'",
                      vm->initrd.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_TTYP])) {
        nm_str_format(&query, "UPDATE vms SET tty_path='%s' WHERE name='%s'",
                      vm->tty.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SOCK])) {
        nm_str_format(&query, "UPDATE vms SET socket_path='%s' WHERE name='%s'",
                      vm->socket.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DEBP])) {
        nm_str_format(&query, "UPDATE vms SET debug_port='%s' WHERE name='%s'",
                      vm->debug_port.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DEBF])) {
        nm_str_format(&query,
                      "UPDATE vms SET debug_freeze='%s' WHERE name='%s'",
                      vm->debug_freeze ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
