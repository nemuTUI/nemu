#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_clone_vm.h>

#define NM_FLD_COUNT 2
static const char NM_CLONE_NAME_MSG[] = "Name";

static void nm_clone_vm_init_windows(nm_form_t *form);
static void nm_clone_vm_to_fs(const nm_str_t *src, const nm_str_t *dst,
                              const nm_vect_t *drives);
static void nm_clone_vm_to_db(const nm_str_t *src, const nm_str_t *dst,
                              const nm_vmctl_data_t *vm);

static void nm_clone_vm_init_windows(nm_form_t *form)
{
    nm_form_window_init();
    if(form) {
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
        if(form_data)
            form_data->parent_window = action_window;
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_CLONE));
    nm_init_help_clone();

    nm_print_vm_menu(NULL);
}

void nm_clone_vm(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_field_t *fields[NM_FLD_COUNT + 1];
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t cl_name = NM_INIT_STR;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_vect_t err = NM_INIT_VECT;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_clone_vm_init_windows(NULL);

    nm_vmctl_get_data(name, &vm);

    msg_len = mbstowcs(NULL, _(NM_CLONE_NAME_MSG), strlen(_(NM_CLONE_NAME_MSG)));

    form_data = nm_form_data_new(
        action_window, nm_clone_vm_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    fields[0] = nm_field_new(NM_FIELD_LABEL, 0, form_data);
    fields[1] = nm_field_new(NM_FIELD_EDIT, 0, form_data);
    fields[2] = NULL;

    set_field_buffer(fields[0], 0, _(NM_CLONE_NAME_MSG));
    set_field_type(fields[1], TYPE_REGEXP, "^[a-zA-Z0-9_-]{1,30} *$");
    nm_str_format(&buf, "%s-clone", name->data);
    set_field_buffer(fields[1], 0, buf.data);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[0], &cl_name);
    nm_form_check_data(_(NM_CLONE_NAME_MSG), cl_name, err);

    if (nm_print_empty_fields(&err) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    if (nm_form_name_used(&cl_name) == NM_ERR)
        goto out;

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_clone_vm_to_fs(name, &cl_name, &vm.drives);
    nm_clone_vm_to_db(name, &cl_name, &vm);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    NM_FORM_EXIT();
    nm_vmctl_free_data(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_str_free(&buf);
    nm_str_free(&cl_name);
}

static void nm_clone_vm_to_fs(const nm_str_t *src, const nm_str_t *dst,
                              const nm_vect_t *drives)
{
    nm_str_t old_vm_path = NM_INIT_STR;
    nm_str_t new_vm_path = NM_INIT_STR;
    nm_str_t new_vm_dir = NM_INIT_STR;
    size_t drives_count;
    char drv_ch = 'a';

    nm_str_format(&new_vm_dir, "%s/%s", nm_cfg_get()->vm_dir.data, dst->data);

    if (mkdir(new_vm_dir.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
        nm_bug(_("%s: cannot create VM directory %s: %s"),
               __func__, new_vm_dir.data, strerror(errno));
    }

    nm_str_format(&new_vm_path, "%s/%s",
        new_vm_dir.data, dst->data);
    nm_str_format(&old_vm_path, "%s/%s/",
        nm_cfg_get()->vm_dir.data, src->data);

    drives_count = drives->n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++) {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        nm_str_t *drive_name = nm_vect_str(drives, NM_SQL_DRV_NAME + idx_shift);

        nm_str_add_str(&old_vm_path, drive_name);
        nm_str_append_format(&new_vm_path, "_%c.img", drv_ch);

        nm_copy_file(&old_vm_path, &new_vm_path);

        nm_str_trunc(&old_vm_path, old_vm_path.len - drive_name->len);
        nm_str_trunc(&new_vm_path, new_vm_path.len - 6);
        drv_ch++;
    }

    nm_str_free(&old_vm_path);
    nm_str_free(&new_vm_path);
    nm_str_free(&new_vm_dir);
}

static void nm_clone_vm_to_db(const nm_str_t *src, const nm_str_t *dst,
                              const nm_vmctl_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;
    uint64_t last_mac;
    uint32_t last_vnc;
    size_t ifs_count;
    size_t drives_count;
    char drv_ch = 'a';

    last_mac = nm_form_get_last_mac();
    last_vnc = nm_form_get_free_vnc();

    nm_str_format(&query, NM_CLONE_VMS_SQL, dst->data, last_vnc, src->data);
    nm_db_edit(query.data);

    /* insert network interface info */
    ifs_count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;

    for (size_t n = 0; n < ifs_count; n++) {
        int altname;
        size_t idx_shift = NM_IFS_IDX_COUNT * n;
        nm_str_t if_name = NM_INIT_STR;
        nm_str_t if_name_copy = NM_INIT_STR;
        nm_str_t maddr = NM_INIT_STR;
        last_mac++;

        nm_net_mac_n2s(last_mac, &maddr);
        nm_str_format(&if_name, "%s_eth%zu", dst->data, n);
        nm_str_copy(&if_name_copy, &if_name);
        altname = nm_net_fix_tap_name(&if_name, &maddr);

        nm_str_format(&query,
            "INSERT INTO ifaces(vm_name, if_name, mac_addr, if_drv, vhost, "
            "macvtap, parent_eth, altname, netuser) "
            "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', 0)",
            dst->data, if_name.data, maddr.data,
            nm_vect_str(&vm->ifs, NM_SQL_IF_DRV + idx_shift)->data,
            nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift)->data,
            nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift)->data,
            nm_vect_str(&vm->ifs, NM_SQL_IF_PET + idx_shift)->data,
            (altname) ? if_name_copy.data : "");
        nm_db_edit(query.data);

        nm_str_free(&if_name);
        nm_str_free(&if_name_copy);
        nm_str_free(&maddr);
    }

    /* insert drive info */
    drives_count = vm->drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++) {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;

        nm_str_format(&query,
            "INSERT INTO drives(vm_name, drive_name, drive_drv, capacity, boot, discard) "
            "VALUES('%s', '%s_%c.img', '%s', '%s', '%s', '%s')",
            dst->data, dst->data, drv_ch,
            nm_vect_str(&vm->drives, NM_SQL_DRV_TYPE + idx_shift)->data,
            nm_vect_str(&vm->drives, NM_SQL_DRV_SIZE + idx_shift)->data,
            nm_vect_str(&vm->drives, NM_SQL_DRV_BOOT + idx_shift)->data,
            nm_vect_str(&vm->drives, NM_SQL_DRV_DISC + idx_shift)->data);
        nm_db_edit(query.data);

        drv_ch++;
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
