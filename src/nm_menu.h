#ifndef NM_MENU_H_
#define NM_MENU_H_

#include <nm_string.h>
#include <nm_vector.h>
#include <nm_ncurses.h>

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

void nm_print_base_menu(nm_menu_data_t *ifs);
void nm_print_vm_menu(nm_menu_data_t *vm);
void nm_print_veth_menu(nm_menu_data_t *veth, int get_status);
void nm_menu_scroll(nm_menu_data_t *menu, size_t list_len, int ch);

#define NM_INIT_MENU_DATA { NULL, 0, 0, 0 }
#define NM_INIT_MENU_ITEM { NULL, 0 }

#endif /* NM_MENU_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
