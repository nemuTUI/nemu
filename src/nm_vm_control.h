#ifndef NM_VM_CONTROL_H_
#define NM_VM_CONTROL_H_

#include <nm_string.h>
#include <nm_vector.h>

enum vmctl_flags {
    NM_VMCTL_TEMP   = (1 << 1),
    NM_VMCTL_INFO   = (1 << 2)
};

typedef struct {
    nm_vect_t   main;
    nm_vect_t   ifs;
    nm_vect_t   drives;
    nm_vect_t   usb;
} nm_vmctl_data_t;

#define NM_VMCTL_INIT_DATA (nm_vmctl_data_t) { \
        NM_INIT_VECT, NM_INIT_VECT, \
        NM_INIT_VECT, NM_INIT_VECT }

void nm_vmctl_start(const nm_str_t *name, int flags);
void nm_vmctl_delete(const nm_str_t *name);
void nm_vmctl_kill(const nm_str_t *name);
void nm_vmctl_get_data(const nm_str_t *name, nm_vmctl_data_t *vm);
void nm_vmctl_free_data(nm_vmctl_data_t *vm);
void nm_vmctl_clear_tap(const nm_str_t *name);
void nm_vmctl_clear_all_tap(void);
void nm_vmctl_gen_cmd(nm_vect_t *argv, const nm_vmctl_data_t *vm,
                      const nm_str_t *name, int flags, nm_vect_t *tfds);
void nm_vmctl_log_last(const nm_str_t *msg);
#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
void nm_vmctl_connect(const nm_str_t *name);
#endif

#endif /*NM_VM_CONTROL_H_ */
/* vim:set ts=4 sw=4: */
