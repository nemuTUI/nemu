#include <nm_core.h>
#include <nm_database.h>
#include <nm_vm_control.h>

static void nm_vmctl_gen_cmd(nm_str_t *res, const nm_vmctl_data_t *vm, int flags);

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
    nm_vmctl_gen_cmd(&cmd, &vm, flags);

    nm_str_free(&cmd);
    nm_vmctl_free_data(&vm);
}

static void nm_vmctl_gen_cmd(nm_str_t *res, const nm_vmctl_data_t *vm, int flags)
{
    //...
}

void nm_vmctl_free_data(nm_vmctl_data_t *vm)
{
    nm_vect_free(&vm->main, nm_str_vect_free_cb);
    nm_vect_free(&vm->ifs, nm_str_vect_free_cb);
    nm_vect_free(&vm->drives, nm_str_vect_free_cb);
}

/* vim:set ts=4 sw=4 fdm=marker: */
