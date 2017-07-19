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

#define NM_EDIT_TITLE "ESC - cancel, F2 - OK"

enum nm_key {
    NM_KEY_ENTER = 10,
    NM_KEY_A = 97,
    NM_KEY_B = 98,
    NM_KEY_C = 99,
    NM_KEY_D = 100,
    NM_KEY_E = 101,
    NM_KEY_F = 102,
    NM_KEY_H = 104,
    NM_KEY_I = 105,
    NM_KEY_K = 107,
    NM_KEY_L = 108,
    NM_KEY_M = 109,
    NM_KEY_O = 111,
    NM_KEY_P = 112,
    NM_KEY_R = 114,
    NM_KEY_S = 115,
    NM_KEY_T = 116,
    NM_KEY_U = 117,
    NM_KEY_V = 118,
    NM_KEY_X = 120,
    NM_KEY_Y = 121,
    NM_KEY_Z = 122
};

#endif /* NM_WINDOW_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
