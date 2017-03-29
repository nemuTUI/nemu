#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_hw_info.h>
#include <nm_cfg_file.h>
#include <nm_usb_devices.h>

#define NM_ADD_VM_FIELDS_NUM 11

static nm_window_t *window = NULL;
static nm_form_t *form = NULL;

enum {
    NM_FLD_VMNAME = 0,
    NM_FLD_VMARCH,
    NM_FLD_CPUNUM,
    NM_FLD_RAMTOT,
    NM_FLD_DISKSZ,
    NM_FLD_DISKIN,
    NM_FLD_SOURCE,
    NM_FLD_IFSCNT,
    NM_FLD_IFSDRV,
    NM_FLD_USBUSE,
    NM_FLD_USBDEV
};

void nm_add_vm(void)
{
    nm_field_t *fields[NM_ADD_VM_FIELDS_NUM + 1];
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t usb_devs = NM_INIT_VECT;
    nm_vect_t usb_names = NM_INIT_VECT;

    nm_usb_get_devs(&usb_devs);
    for (size_t n = 0; n < usb_devs.n_memb; n++)
    {
        nm_vect_insert(&usb_names,
                       nm_usb_name(usb_devs.data[n]).data,
                       nm_usb_name(usb_devs.data[n]).len + 1,
                       NULL);
#if (NM_DEBUG)
        nm_debug("%s %s\n", nm_usb_id(usb_devs.data[n]).data,
                 nm_usb_name(usb_devs.data[n]).data);
#endif
    }

    for (size_t n = 0; n < usb_names.n_memb; n++)
        nm_debug("u: %s\n", (char *)usb_names.data[n]);

    nm_vect_end_zero(&usb_names);
    
    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(25, 67, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_ADD_VM_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 38, n * 2, 5, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_ADD_VM_FIELDS_NUM] = NULL;

    set_field_type(fields[NM_FLD_VMNAME], TYPE_REGEXP, "^[a-zA-Z0-9_-]* *$");
    set_field_type(fields[NM_FLD_VMARCH], TYPE_ENUM, nm_cfg_get_arch(), false, false);
    set_field_type(fields[NM_FLD_CPUNUM], TYPE_INTEGER, 0, 1, nm_hw_ncpus());
    set_field_type(fields[NM_FLD_RAMTOT], TYPE_INTEGER, 0, 4, nm_hw_total_ram());
    set_field_type(fields[NM_FLD_DISKSZ], TYPE_INTEGER, 0, 1, nm_hw_disk_free());
    set_field_type(fields[NM_FLD_DISKIN], TYPE_ENUM, nm_form_drive_drv, false, false);
    set_field_type(fields[NM_FLD_SOURCE], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_IFSCNT], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[NM_FLD_IFSDRV], TYPE_ENUM, nm_form_net_drv, false, false);
    set_field_type(fields[NM_FLD_USBUSE], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_USBDEV], TYPE_ENUM, usb_names.data, false, false);

    if (usb_devs.n_memb == 0)
    {
        field_opts_off(fields[NM_FLD_USBUSE], O_ACTIVE);
        field_opts_off(fields[NM_FLD_USBDEV], O_ACTIVE);
    }

    set_field_buffer(fields[NM_FLD_VMARCH], 0, *nm_cfg_get()->qemu_targets.data);
    set_field_buffer(fields[NM_FLD_CPUNUM], 0, "1");
    set_field_buffer(fields[NM_FLD_DISKIN], 0, NM_DEFAULT_DRVINT);
    set_field_buffer(fields[NM_FLD_IFSCNT], 0, "1");
    set_field_buffer(fields[NM_FLD_IFSDRV], 0, NM_DEFAULT_NETDRV);
    set_field_buffer(fields[NM_FLD_USBUSE], 0, "no");
    field_opts_off(fields[NM_FLD_VMNAME], O_STATIC);
    field_opts_off(fields[NM_FLD_SOURCE], O_STATIC);
    field_opts_off(fields[NM_FLD_USBDEV], O_STATIC);
    set_max_field(fields[NM_FLD_VMNAME], 30);

    mvwaddstr(window, 2, 2, _("Name"));
    mvwaddstr(window, 4, 2, _("Architecture"));

    nm_str_alloc_text(&buf, _("CPU cores [1-"));
    nm_str_format(&buf, "%u", nm_hw_ncpus());
    nm_str_add_char(&buf, ']');
    mvwaddstr(window, 6, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Memory [4-"));
    nm_str_format(&buf, "%u", nm_hw_total_ram());
    nm_str_add_text(&buf, "]Mb");
    mvwaddstr(window, 8, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Disk [1-"));
    nm_str_format(&buf, "%u", nm_hw_disk_free());
    nm_str_add_text(&buf, "]Gb");
    mvwaddstr(window, 10, 2, buf.data);
    nm_str_trunc(&buf, 0);

    mvwaddstr(window, 12, 2, _("Disk interface"));
    mvwaddstr(window, 14, 2, _("Path to ISO/IMG"));
    mvwaddstr(window, 16, 2, _("Network interfaces"));
    mvwaddstr(window, 18, 2, _("Net driver"));
    mvwaddstr(window, 20, 2, _("USB [yes/no]"));
    mvwaddstr(window, 22, 2, _("USB device"));

    form = nm_post_form(window, fields, 21);
    nm_draw_form(window, form);

    /* cleanup */
    nm_form_free(form, fields);
    nm_str_free(&buf);
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&usb_devs, nm_usb_vect_free_cb);
}

/* vim:set ts=4 sw=4 fdm=marker: */
