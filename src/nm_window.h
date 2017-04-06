#ifndef NM_WINDOW_H_
#define NM_WINDOW_H_

#include <nm_ncurses.h>

void nm_print_main_window(void);
void nm_print_vm_window(void);
int nm_print_warn(int nlines, int begin_x, const char *msg);
void nm_print_vm_info(const nm_str_t *name);
void nm_print_cmd(const nm_str_t *name);
void nm_print_help(nm_window_t *w);
void nm_print_nemu(void);
void nm_print_title(const char *msg);

#define NM_EDIT_TITLE "F10 - cancel, F2 - OK"

#endif /* NM_WINDOW_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
