#ifndef NM_FORM_H_
#define NM_FORM_H_

#include <nm_string.h>
#include <nm_vector.h>
#include <nm_ncurses.h>

#include <form.h>
#include <pthread.h>

#define NM_FORM_EMPTY_MSG  "These fields cannot be empty:"
#define NM_FORM_VMNAME_MSG "This name is already used"

typedef FORM nm_form_t;
typedef FIELD nm_field_t;

typedef struct {
    nm_str_t driver;
    nm_str_t size;
} nm_vm_drive_t;

typedef struct {
    nm_str_t driver;
    uint32_t count;
} nm_vm_ifs_t;

typedef struct {
    nm_str_t device;
    uint32_t enable:1;
} nm_vm_usb_t;

typedef struct {
    uint32_t enable:1;
    uint32_t hostcpu_enable:1;
} nm_vm_kvm_t;

typedef struct {
    nm_str_t name;
    nm_str_t arch;
    nm_str_t cpus;
    nm_str_t memo;
    nm_str_t srcp;
    nm_str_t vncp;
    nm_vm_usb_t usb;
    nm_vm_drive_t drive;
    nm_vm_ifs_t ifs;
    nm_vm_kvm_t kvm;
    uint32_t mouse_sync:1;
} nm_vm_t;

typedef struct {
    int x;
    int y;
    const int *stop;
} nm_spinner_data_t;

nm_form_t *nm_post_form(nm_window_t *w, nm_field_t **field, int begin_x);
int nm_draw_form(nm_window_t *w, nm_form_t *form);
void nm_form_free(nm_form_t *form, nm_field_t **fields);
void nm_get_field_buf(nm_field_t *f, nm_str_t *res);
void nm_vm_get_usb(nm_vect_t *devs, nm_vect_t *names);
int nm_form_name_used(const nm_str_t *name);
int nm_print_empty_fields(const nm_vect_t *v);
void nm_vm_free(nm_vm_t *vm);
void *nm_spinner(void *data);

extern const char *nm_form_yes_no[];
extern const char *nm_form_net_drv[];
extern const char *nm_form_drive_drv[];

#define nm_form_check_data(name, val, v)                      \
    {                                                         \
        if (val.len == 0)                                     \
            nm_vect_insert(&v, name, strlen(name) + 1, NULL); \
    }

#define NM_INIT_VM_DRIVE { NM_INIT_STR, NM_INIT_STR }
#define NM_INIT_VM_IFS   { NM_INIT_STR, 0 }
#define NM_INIT_VM_USB   { NM_INIT_STR, 0 }
#define NM_INIT_VM_KVM   { 0, 0 }
#define NM_INIT_SPINNER  { 0, 1, NULL }

#define NM_INIT_VM { NM_INIT_STR, NM_INIT_STR, NM_INIT_STR, \
                     NM_INIT_STR, NM_INIT_STR, NM_INIT_STR, \
                     NM_INIT_VM_USB, NM_INIT_VM_DRIVE, NM_INIT_VM_IFS, \
                     NM_INIT_VM_KVM, 0 }

#endif /* NM_FORM_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
