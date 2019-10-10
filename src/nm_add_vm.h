#ifndef NM_ADD_VM_H_
#define NM_ADD_VM_H_

#include <nm_form.h>

void nm_add_vm(void);
void nm_import_vm(void);
void nm_add_vm_to_db(nm_vm_t *vm, uint64_t mac,
                     int import, const nm_vect_t *drives);

enum {
    NM_INSTALL_VM = 0,
    NM_IMPORT_VM
};

#endif /* NM_ADD_VM_H_ */
/* vim:set ts=4 sw=4: */
