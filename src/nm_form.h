#ifndef NM_FORM_H_
#define NM_FORM_H_

#include <nm_string.h>
#include <nm_vector.h>
#include <nm_ncurses.h>
#include <nm_usb_devices.h>

#include <form.h>
#include <pthread.h>

typedef FORM nm_form_t;
typedef FIELD nm_field_t;

typedef enum {
    NM_FIELD_DEFAULT = 0,
    NM_FIELD_LABEL,
    NM_FIELD_EDIT
} nm_field_type_t;

typedef struct {
    nm_field_type_t type;
    size_t row;
    nm_vect_t children;
    void (*on_change)(nm_field_t *);
} nm_field_data_t;

typedef struct {
    nm_str_t driver;
    nm_str_t size;
    uint32_t discard:1;
} nm_vm_drive_t;

#define NM_INIT_VM_DRIVE (nm_vm_drive_t) { NM_INIT_STR, NM_INIT_STR, 0 }

typedef struct {
    int field_hpad;
    int field_vpad;
    int form_hpad;
    int form_vpad;
    float form_ratio;
    int min_edit_size;

    size_t form_len;
    size_t w_start_x;
    size_t w_start_y;
    size_t w_cols;
    size_t w_rows;
    size_t field_lines;
    size_t msg_len;
    int color;
    nm_window_t *parent_window;
    nm_window_t *form_window;
    void (*on_redraw)(nm_form_t *);
} nm_form_data_t;

#define NM_INIT_FORM_DATA (nm_form_data_t) { 0, 0, 0, 0, NULL }

typedef struct {
    nm_str_t driver;
    uint32_t count;
} nm_vm_ifs_t;

#define NM_INIT_VM_IFS (nm_vm_ifs_t) { NM_INIT_STR, 0 }

typedef struct {
    nm_str_t name;
    nm_usb_dev_t *device;
} nm_vm_usb_t;

#define NM_INIT_VM_USB (nm_vm_usb_t) { NM_INIT_STR, NULL, 0 }

typedef struct {
    uint32_t enable:1;
    uint32_t hostcpu_enable:1;
} nm_vm_kvm_t;

#define NM_INIT_VM_KVM (nm_vm_kvm_t) { 0, 0 }

typedef struct {
    nm_str_t inst_path;
    nm_str_t bios;
    nm_str_t kernel;
    nm_str_t cmdline;
    nm_str_t initrd;
    nm_str_t debug_port;
    uint32_t installed:1;
    uint32_t debug_freeze:1;
} nm_vm_boot_t;

#define NM_INIT_VM_BOOT (nm_vm_boot_t) { \
                         NM_INIT_STR, NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR, NM_INIT_STR, NM_INIT_STR, \
                         0, 0 }

typedef struct {
    nm_str_t name;
    nm_str_t arch;
    nm_str_t cpus;
    nm_str_t memo;
    nm_str_t srcp;
    nm_str_t vncp;
    nm_str_t mach;
    nm_str_t cmdappend;
    nm_str_t group;
    nm_str_t usb_type;
    nm_vm_drive_t drive;
    nm_vm_ifs_t ifs;
    nm_vm_kvm_t kvm;
    uint32_t usb_enable:1;
} nm_vm_t;

#define NM_INIT_VM (nm_vm_t) { \
                    NM_INIT_STR, NM_INIT_STR, NM_INIT_STR,         \
                    NM_INIT_STR, NM_INIT_STR, NM_INIT_STR,         \
                    NM_INIT_STR, NM_INIT_STR, NM_INIT_STR,         \
                    NM_INIT_STR, NM_INIT_VM_DRIVE, NM_INIT_VM_IFS, \
                    NM_INIT_VM_KVM, 0 }

typedef struct {
    const int *stop;
    const void *ctx;
} nm_spinner_data_t;

#define NM_INIT_SPINNER (nm_spinner_data_t) { NULL, NULL }

nm_field_t *nm_field_new(nm_field_type_t type, int row, nm_form_data_t *form_data);
void nm_field_free(nm_field_t *field);
void nm_fields_free(nm_field_t **fields);

nm_form_data_t *nm_form_data_new(
    nm_window_t *parent, void (*on_redraw)(nm_form_t *),
    size_t msg_len, size_t field_lines, int color
);
int nm_form_data_update(nm_form_data_t *form_data, size_t msg_len, size_t field_lines);
void nm_form_data_free(nm_form_data_t *form_data);

nm_form_t *nm_form_new(nm_form_data_t *form_data, nm_field_t **field);
void nm_form_window_init();
void nm_form_post(nm_form_t *form);
int nm_form_draw(nm_form_t **form);
void nm_form_free(nm_form_t *form);

void nm_get_field_buf(nm_field_t *f, nm_str_t *res);
int nm_form_name_used(const nm_str_t *name);
uint64_t nm_form_get_last_mac(void);
uint32_t nm_form_get_free_vnc(void);
int nm_print_empty_fields(const nm_vect_t *v);
void nm_vm_free(nm_vm_t *vm);
void nm_vm_free_boot(nm_vm_boot_t *vm);
void *nm_progress_bar(void *data);
void *nm_file_progress(void *data);

extern const char *nm_form_yes_no[];
extern const char *nm_form_net_drv[];
extern const char *nm_form_drive_drv[];
extern const char *nm_form_macvtap[];
extern const char *nm_form_usbtype[];
extern const char *nm_form_svg_layer[];
extern const char *nm_form_displaytype[];

#define NM_FORM_RESET()                                       \
    do {                                                      \
        curs_set(0);                                          \
        wtimeout(action_window, -1);                          \
    } while (0)

#define NM_FORM_EXIT()                                        \
    do {                                                      \
        wtimeout(action_window, -1);                          \
        werase(help_window);                                  \
        nm_init_help_main();                                  \
    } while (0)

#define nm_form_check_data(name, val, v)                      \
    {                                                         \
        if (val.len == 0)                                     \
            nm_vect_insert(&v, name, strlen(name) + 1, NULL); \
    }

#define nm_form_check_datap(name, val, v)                     \
    {                                                         \
        if (val->len == 0)                                    \
            nm_vect_insert(&v, name, strlen(name) + 1, NULL); \
    }

#endif /* NM_FORM_H_ */
/* vim:set ts=4 sw=4: */
