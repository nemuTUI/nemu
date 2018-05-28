#ifndef NM_MENU_H_
#define NM_MENU_H_

#include <nm_string.h>
#include <nm_vector.h>
#include <nm_ncurses.h>

enum {
    NM_CHOICE_VM_LIST = 1,
    NM_CHOICE_VM_INST,
    NM_CHOICE_VM_IMPORT,
#if defined (NM_WITH_OVF_SUPPORT)
    NM_CHOICE_OVF_IMPORT,
#endif
#if defined (NM_OS_LINUX)
    NM_CHOICE_NETWORK,
#endif
    NM_CHOICE_QUIT,
    NM_MAIN_CHOICES = NM_CHOICE_QUIT
};

typedef struct {
    nm_vect_t *v;
    size_t item_first;
    size_t item_last;
    uint32_t highlight;
} nm_menu_data_t;

typedef struct {
    const nm_str_t *name;
    uint32_t status:1;
} nm_menu_item_t;

void nm_print_main_menu(nm_window_t *w, uint32_t highlight);
void nm_print_vm_menu(nm_menu_data_t *vm);
void nm_print_veth_menu(nm_window_t *w, nm_menu_data_t *veth, int get_status);

#define NM_VM_RUNNING "running"
#define NM_VM_STOPPED "stopped"
#define NM_VETH_UP   "  up"
#define NM_VETH_DOWN "down"

#define NM_INIT_MENU_DATA { NULL, 0, 0, 0 }
#define NM_INIT_MENU_ITEM { NULL, 0 }

#endif /* NM_MENU_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
