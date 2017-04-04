#ifndef NM_MENU_H_
#define NM_MENU_H_

#include <nm_string.h>
#include <nm_vector.h>
#include <nm_ncurses.h>

typedef struct {
    nm_vect_t *v;
    size_t vm_first;
    size_t vm_last;
    uint32_t highlight;
} nm_vm_list_t;

typedef struct {
    const nm_str_t *name;
    uint32_t status:1;
} nm_vm_t;

void nm_print_main_menu(nm_window_t *w, uint32_t highlight);
void nm_print_vm_menu(nm_window_t *w, nm_vm_list_t *vm);

#define NM_VM_RUNNING "running"
#define NM_VM_STOPPED "stopped"

#define NM_INIT_VMS_LIST { NULL, 0, 0, 0 }
#define NM_INIT_VM  { NULL, 0 }

#endif /* NM_MENU_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
