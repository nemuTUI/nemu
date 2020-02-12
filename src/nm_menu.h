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

#define NM_INIT_MENU_DATA (nm_menu_data_t) { NULL, 0, 0, 0 }

typedef struct {
    const nm_str_t *name;
    uint32_t status:2;
} nm_menu_item_t;

#define NM_INIT_MENU_ITEM (nm_menu_item_t) { NULL, 2 }

void nm_print_base_menu(nm_menu_data_t *ifs);
void nm_print_vm_menu(nm_menu_data_t *vm);
void nm_print_veth_menu(nm_menu_data_t *veth, int get_status);
void nm_menu_scroll(nm_menu_data_t *menu, size_t list_len, int ch);

static inline nm_menu_item_t *nm_vect_item(const nm_vect_t *v, const size_t index)
{
    return (nm_menu_item_t *)nm_vect_at(v, index);
}
static inline nm_str_t *nm_vect_item_name(const nm_vect_t *v, const size_t index)
{
    return (nm_str_t*)nm_vect_item(v, index)->name;
}
static inline char *nm_vect_item_name_ctx(const nm_vect_t *v, const size_t index)
{
    return nm_vect_item_name(v, index)->data;
}
static inline nm_str_t *nm_vect_item_name_cur(const nm_menu_data_t *p)
{
    return nm_vect_item_name(p->v, (p->item_first + p->highlight) - 1);
}
static inline int nm_vect_item_status(const nm_vect_t *v, const size_t index)
{
    return nm_vect_item(v, index)->status;
}
static inline int nm_vect_item_status_cur(const nm_menu_data_t *p)
{
    return nm_vect_item_status(p->v, (p->item_first + p->highlight) - 1);
}
static inline void nm_vect_set_item_status(nm_vect_t *v, const size_t index, const int s)
{
    nm_vect_item(v, index)->status = s;
}

#endif /* NM_MENU_H_ */
/* vim:set ts=4 sw=4: */
