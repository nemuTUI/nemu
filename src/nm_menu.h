#ifndef NM_MENU_H_
#define NM_MENU_H_

#include <nm_vector.h>

typedef struct {
    const nm_vect_t *v;
    size_t vm_first;
    size_t vm_last;
    uint32_t highlight;
} nm_vm_list_t;

void nm_print_main_menu(nm_window_t *w, uint32_t highlight);
void nm_print_vm_menu(nm_window_t *w, const nm_vm_list_t *vm);

#define NM_INIT_VM_LITST { NULL, 0, 0, 0 }

#endif /* NM_MENU_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
