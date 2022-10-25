#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_database.h>
#include <nm_vm_control.h>
#include <nm_edit_boot.h>

static const char NM_LC_EDIT_BOOT_FORM_INST[] = "OS Installed";
static const char NM_LC_EDIT_BOOT_FORM_SRCP[] = "Path to ISO/IMG";
static const char NM_LC_EDIT_BOOT_FORM_BIOS[] = "Path to BIOS";
static const char NM_LC_EDIT_BOOT_FORM_KERN[] = "Path to kernel";
static const char NM_LC_EDIT_BOOT_FORM_CMDL[] = "Kernel cmdline";
static const char NM_LC_EDIT_BOOT_FORM_INIT[] = "Path to initrd";
static const char NM_LC_EDIT_BOOT_FORM_DEBP[] = "GDB debug port";
static const char NM_LC_EDIT_BOOT_FORM_DEBF[] = "Freeze after start";

static void nm_edit_boot_init_windows(nm_form_t *form);
static void nm_edit_boot_fields_setup(const nm_vmctl_data_t *cur);
static size_t nm_edit_boot_labels_setup(void);
static int nm_edit_boot_get_data(nm_vm_boot_t *vm);
static void nm_edit_boot_update_db(const nm_str_t *name, nm_vm_boot_t *vm);

enum {
    NM_LBL_INST = 0,  NM_FLD_INST,
    NM_LBL_SRCP, NM_FLD_SRCP,
    NM_LBL_BIOS, NM_FLD_BIOS,
    NM_LBL_KERN, NM_FLD_KERN,
    NM_LBL_CMDL, NM_FLD_CMDL,
    NM_LBL_INIT, NM_FLD_INIT,
    NM_LBL_DEBP, NM_FLD_DEBP,
    NM_LBL_DEBF, NM_FLD_DEBF,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_edit_boot_init_windows(nm_form_t *form)
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
    nm_init_action(_(NM_MSG_EDIT_BOOT));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_edit_boot(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vm_boot_t vm = NM_INIT_VM_BOOT;
    nm_vmctl_data_t cur_settings = NM_VMCTL_INIT_DATA;
    size_t msg_len;

    nm_edit_boot_init_windows(NULL);

    nm_vmctl_get_data(name, &cur_settings);

    msg_len = nm_edit_boot_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_edit_boot_init_windows, msg_len,
        NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_FLD_INST:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        case NM_FLD_SRCP:
            fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
            break;
        case NM_FLD_BIOS:
            fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
            break;
        case NM_FLD_KERN:
            fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
            break;
        case NM_FLD_CMDL:
            fields[n] = nm_field_regexp_new(n / 2, form_data, ".*");
            break;
        case NM_FLD_INIT:
            fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
            break;
        case NM_FLD_DEBP:
            fields[n] = nm_field_integer_new(n / 2, form_data,
                    1, 0, 0xffff);
            break;
        case NM_FLD_DEBF:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        default:
            fields[n] = nm_field_label_new(n / 2, form_data);
            break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_edit_boot_labels_setup();
    nm_edit_boot_fields_setup(&cur_settings);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    if (nm_edit_boot_get_data(&vm) != NM_OK) {
        goto out;
    }

    nm_edit_boot_update_db(name, &vm);

out:
    NM_FORM_EXIT();
    nm_vmctl_free_data(&cur_settings);
    nm_vm_free_boot(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
}

static void nm_edit_boot_fields_setup(const nm_vmctl_data_t *cur)
{
    for (size_t n = 1; n < NM_FLD_COUNT; n++) {
        field_opts_off(fields[n], O_STATIC);
    }

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_INST),
                NM_ENABLE) == NM_OK) {
        set_field_buffer(fields[NM_FLD_INST], 0, nm_form_yes_no[1]);
    } else {
        set_field_buffer(fields[NM_FLD_INST], 0, nm_form_yes_no[0]);
    }

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
    set_field_buffer(fields[NM_FLD_DEBP], 0,
            nm_vect_str_ctx(&cur->main, NM_SQL_DEBP));

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_DEBF),
                NM_ENABLE) == NM_OK) {
        set_field_buffer(fields[NM_FLD_DEBF], 0, nm_form_yes_no[0]);
    } else {
        set_field_buffer(fields[NM_FLD_DEBF], 0, nm_form_yes_no[1]);
    }
}

static size_t nm_edit_boot_labels_setup(void)
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_INST:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_INST));
            break;
        case NM_LBL_SRCP:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_SRCP));
            break;
        case NM_LBL_BIOS:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_BIOS));
            break;
        case NM_LBL_KERN:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_KERN));
            break;
        case NM_LBL_CMDL:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_CMDL));
            break;
        case NM_LBL_INIT:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_INIT));
            break;
        case NM_LBL_DEBP:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_DEBP));
            break;
        case NM_LBL_DEBF:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_BOOT_FORM_DEBF));
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

static int nm_edit_boot_get_data(nm_vm_boot_t *vm)
{
    int rc;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t inst = NM_INIT_STR;
    nm_str_t debug_freeze = NM_INIT_STR;

    nm_get_field_buf(fields[NM_FLD_INST], &inst);
    nm_get_field_buf(fields[NM_FLD_SRCP], &vm->inst_path);
    nm_get_field_buf(fields[NM_FLD_BIOS], &vm->bios);
    nm_get_field_buf(fields[NM_FLD_KERN], &vm->kernel);
    nm_get_field_buf(fields[NM_FLD_CMDL], &vm->cmdline);
    nm_get_field_buf(fields[NM_FLD_INIT], &vm->initrd);
    nm_get_field_buf(fields[NM_FLD_DEBP], &vm->debug_port);
    nm_get_field_buf(fields[NM_FLD_DEBF], &debug_freeze);

    if (field_status(fields[NM_FLD_INST])) {
        nm_form_check_data(_("OS Installed"), inst, err);
    }

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR) {
        goto out;
    }

    if (nm_str_cmp_st(&inst, "no") == NM_OK) {
        vm->installed = 1;

        if (vm->inst_path.len == 0) {
            nm_warn(_(NM_MSG_ISO_MISS));
            rc = NM_ERR;
            goto out;
        }
    }

    if (nm_str_cmp_st(&debug_freeze, "yes") == NM_OK) {
        vm->debug_freeze = 1;
    } else {
        vm->debug_freeze = 0;
    }

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
        nm_str_format(&query, "UPDATE vms SET kernel_append='%s' "
                "WHERE name='%s'",
                vm->cmdline.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_INIT])) {
        nm_str_format(&query, "UPDATE vms SET initrd='%s' WHERE name='%s'",
                vm->initrd.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DEBP])) {
        nm_str_format(&query, "UPDATE vms SET debug_port='%s' WHERE name='%s'",
                vm->debug_port.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DEBF])) {
        nm_str_format(&query, "UPDATE vms SET debug_freeze='%s' "
                "WHERE name='%s'",
                vm->debug_freeze ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
