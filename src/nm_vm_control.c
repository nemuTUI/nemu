#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

static void nm_vmctl_gen_cmd(nm_str_t *res, const nm_vmctl_data_t *vm,
                             const nm_str_t *name, int flags);

void nm_vmctl_get_data(const nm_str_t *name, nm_vmctl_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_add_text(&query, "SELECT * FROM vms WHERE name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_select(query.data, &vm->main);

    nm_str_trunc(&query, 0);
    nm_str_add_text(&query, "SELECT if_name, mac_addr, if_drv, ipv4_addr "
        "FROM ifaces WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_text(&query, "' ORDER BY if_name ASC");
    nm_db_select(query.data, &vm->ifs);

    nm_str_trunc(&query, 0);
    nm_str_add_text(&query, "SELECT drive_name, drive_drv, capacity, boot "
        "FROM drives WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_text(&query, "' ORDER BY drive_name ASC");
    nm_db_select(query.data, &vm->drives);

    nm_str_free(&query);
}

void nm_vmctl_start(const nm_str_t *name, int flags)
{
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_str_t cmd = NM_INIT_STR;

    nm_vmctl_get_data(name, &vm);

    /* {{{ Check if VM is already installed */
    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_INST), NM_ENABLE) == NM_OK)
    {
        int ch = nm_print_warn(3, 6, _("Already installed (y/n)"));
        if (ch == 'y')
        {
            flags |= NM_VMCTL_INST;
            flags &= ~NM_VMCTL_TEMP;
            nm_str_t query = NM_INIT_STR;

            nm_str_alloc_text(&query, "UPDATE vms SET install='0' WHERE name='");
            nm_str_add_str(&query, name);
            nm_str_add_char(&query, '\'');
            nm_db_edit(query.data);

            nm_str_free(&query);
        }
    }
    /* }}} VM is already installed */

    nm_vmctl_gen_cmd(&cmd, &vm, name, flags);

    nm_str_free(&cmd);
    nm_vmctl_free_data(&vm);
}

static void nm_vmctl_gen_cmd(nm_str_t *res, const nm_vmctl_data_t *vm,
                             const nm_str_t *name, int flags)
{
    nm_str_t vmdir = NM_INIT_STR;

    nm_str_alloc_str(&vmdir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vmdir, '/');
    nm_str_add_str(&vmdir, name);
    nm_str_add_char(&vmdir, '/');

    if (!(flags & NM_VMCTL_INFO))
        nm_str_add_text(res, "( ");

    if (!(flags & NM_VMCTL_INFO))
    {
        nm_str_add_text(res, " > /dev/null 2>&1; rm -f ");
        nm_str_add_str(res, &vmdir);
        nm_str_add_text(res, NM_VM_PID_FILE " )&");
    }

#if (NM_DEBUG)
    nm_debug("cmd=%s\n", res->data);
#endif
    nm_str_free(&vmdir);
}

void nm_vmctl_free_data(nm_vmctl_data_t *vm)
{
    nm_vect_free(&vm->main, nm_str_vect_free_cb);
    nm_vect_free(&vm->ifs, nm_str_vect_free_cb);
    nm_vect_free(&vm->drives, nm_str_vect_free_cb);
}

/* vim:set ts=4 sw=4 fdm=marker: */
