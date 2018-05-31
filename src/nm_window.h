#ifndef NM_WINDOW_H_
#define NM_WINDOW_H_

#include <nm_ncurses.h>
#include <nm_vm_control.h>

void nm_print_main_window(void);
void nm_print_vm_window(void);
int nm_print_warn(int nlines, int begin_x, const char *msg);
void nm_print_vm_info(const nm_str_t *name, const nm_vmctl_data_t *vm);
void nm_print_cmd(const nm_str_t *name);
void nm_print_help(nm_window_t *w);
void nm_print_nemu(void);
void nm_print_title(const char *msg);
void nm_create_windows(void);
void nm_destroy_windows(void);
void nm_init_action(const char *msg);
void nm_init_help(const char *msg, int err);
void nm_init_side(void);
void nm_align2line(nm_str_t *str, size_t line_len);
void nm_warn_small_size(void);
size_t nm_max_msg_len(const char **msg);

/* Help window*/
extern nm_window_t *help_window;
/* Side bar window. Mostly used for VM list */
extern nm_window_t *side_window;
/* Action|information window. */
extern nm_window_t *action_window;
/* Used in SIGWINCH signal handler */
extern sig_atomic_t redraw_window;

#define NM_EDIT_TITLE "ESC - cancel, ENTER - OK"
#define NM_EDIT_HELP  "esc:Cancel enter:Save"
#define NM_SPACES "                         "

#define NM_ERASE_TITLE(t, cols) \
    mvwhline(t ## _window, 1, 1, ' ', (cols) - 2)

enum nm_key {
    NM_KEY_ENTER    = 10,
    NM_KEY_QUESTION = 63,
    NM_KEY_PLUS     = 43,
    NM_KEY_MINUS    = 45,
    NM_KEY_SLASH    = 47,
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
    NM_KEY_Q = 113,
    NM_KEY_R = 114,
    NM_KEY_S = 115,
    NM_KEY_T = 116,
    NM_KEY_U = 117,
    NM_KEY_V = 118,
    NM_KEY_X = 120,
    NM_KEY_Y = 121,
    NM_KEY_Z = 122
};

enum nm_key_upper {
    NM_KEY_D_UP = 68,
    NM_KEY_P_UP = 80,
    NM_KEY_R_UP = 82,
    NM_KEY_S_UP = 83,
    NM_KEY_X_UP = 88
};

#endif /* NM_WINDOW_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
