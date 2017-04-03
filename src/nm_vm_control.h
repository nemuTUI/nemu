#ifndef NM_VM_CONTROL_H_
#define NM_VM_CONTROL_H_

#include <nm_string.h>
#include <nm_vector.h>

enum vmctl_flags {
    VMCTL_START_TEMP = (1 << 1)
};

typedef struct {
    nm_vect_t main;
    nm_vect_t ifs;
    nm_vect_t drives;
} nm_vmctl_data_t;

void nm_vmctl_start(const nm_str_t *name, int flags);
void nm_vmctl_get_data(const nm_str_t *name, nm_vmctl_data_t *vm);
void nm_vmctl_free_data(nm_vmctl_data_t *vm);

#define NM_VMCTL_INIT_DATA { NM_INIT_VECT, NM_INIT_VECT, NM_INIT_VECT }

#endif /*NM_VM_CONTROL_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
