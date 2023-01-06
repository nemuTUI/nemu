#ifndef NM_EDIT_VM_H_
#define NM_EDIT_VM_H_

#include <nm_form.h>
#include <nm_string.h>
#include <nm_vm_control.h>

void nm_edit_vm(const nm_str_t *name);
void nm_edit_update_ifs(nm_str_t *query, const nm_vmctl_data_t *vm_cur,
        const nm_vm_t *vm_new, uint64_t mac);

#endif /*NM_EDIT_VM_H_ */
/* vim:set ts=4 sw=4: */
