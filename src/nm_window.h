#ifndef NM_WINDOW_H_
#define NM_WINDOW_H_

#include <nm_ncurses.h>
#include <nm_vm_control.h>

void nm_print_vm_info(const nm_str_t *name, const nm_vmctl_data_t *vm, int status);
void nm_print_iface_info(const nm_vmctl_data_t *vm, size_t idx);
void nm_print_drive_info(const nm_vect_t *v, size_t idx);
void nm_print_snapshots(const nm_vect_t *v);
void nm_print_cmd(const nm_str_t *name);
void nm_print_help(void);
void nm_lan_help(void);
void nm_print_nemu(void);
void nm_create_windows(void);
void nm_destroy_windows(void);
void nm_init_action(const char *msg);
void nm_init_help(const char *msg, int err);
void nm_init_help_main(void);
void nm_init_help_lan(void);
void nm_init_help_edit(void);
void nm_init_help_iface(void);
void nm_init_help_import(void);
void nm_init_help_install(void);
void nm_init_help_clone(void);
void nm_init_help_export(void);
void nm_init_help_delete(void);
void nm_init_side(void);
void nm_init_side_lan(void);
void nm_init_side_if_list(void);
void nm_init_side_drives(void);
void nm_align2line(nm_str_t *str, size_t line_len);
int nm_warn(const char *msg);
int nm_notify(const char *msg);
size_t nm_max_msg_len(const char **msg);
int nm_window_scale_inc(void);
int nm_window_scale_dec(void);

/* Help window*/
extern nm_window_t *help_window;
/* Side bar window. Mostly used for VM list */
extern nm_window_t *side_window;
/* Action|information window. */
extern nm_window_t *action_window;
/* Used in SIGWINCH signal handler */
extern sig_atomic_t redraw_window;

#define NM_MSG_ANY_KEY    ", press any key"
#define NM_MSG_SMALL_WIN  "Window size to small" NM_MSG_ANY_KEY
#define NM_MSG_NO_IFACES  "No network configured" NM_MSG_ANY_KEY
#define NM_MSG_HCPU_KVM   "Host CPU requires KVM enabled" NM_MSG_ANY_KEY
#define NM_MSG_BAD_CTX    "Contents of field is invalid" NM_MSG_ANY_KEY
#define NM_MSG_NULL_FLD   "These fields cannot be empty:"
#define NM_MSG_NAME_BUSY  "This name is already used" NM_MSG_ANY_KEY
#define NM_MSG_IF_PROP    "Interface properties"
#define NM_MSG_OVA_HEADER "Import OVA image"
#define NM_MSG_MUST_STOP  "VM must be stopped" NM_MSG_ANY_KEY
#define NM_MSG_MUST_RUN   "VM must be running" NM_MSG_ANY_KEY
#define NM_MSG_DELETE     "Confirm deletion? (y/n)"
#define NM_MSG_INSTALL    "Install VM"
#define NM_MSG_IMPORT     "Import drive image"
#define NM_MSG_CLONE      "Clone VM"
#define NM_MSG_EXPORT_MAP "Export network map to SVG"
#define NM_MSG_LAN        "Connected VM's"
#define NM_MSG_EDIT_VM    "Edit properties"
#define NM_MSG_EDIT_BOOT  "Edit boot settings"
#define NM_MSG_9P_SHARE   "Share host filesystem"
#define NM_MSG_USB_ATTACH "Attach USB device"
#define NM_MSG_USB_DETACH "Detach USB device"
#define NM_MSG_SNAP_CRT   "Create VM snapshot"
#define NM_MSG_SNAP_REV   "Revert VM snapshot"
#define NM_MSG_SNAP_DEL   "Delete VM snapshot"
#define NM_MSG_VDRIVE_ADD "Add virtual drive"
#define NM_MSG_VDRIVE_DEL "Delete virtual drive"
#define NM_MSG_ADD_VETH   "Create VETH interface"
#define NM_MSG_INST_CONF  "Already installed (y/n)"
#define NM_MSG_SNAP_OVER  "Override snapshot? (y/n)"
#define NM_MSG_RUNNING    "Already running" NM_MSG_ANY_KEY
#define NM_MSG_NO_SPACE   "No space left for importing drive image" NM_MSG_ANY_KEY
#define NM_MSG_IFCLR_DONE "Unused ifaces deleted" NM_MSG_ANY_KEY
#define NM_MSG_IFCLR_NONE "No unused ifaces" NM_MSG_ANY_KEY
#define NM_MSG_ISO_MISS   "ISO/IMG path not set" NM_MSG_ANY_KEY
#define NM_MSG_ISO_NF     "ISO path does not exist" NM_MSG_ANY_KEY
#define NM_MSG_USB_DIS    "USB must be enabled at boot time" NM_MSG_ANY_KEY
#define NM_MSG_USB_MISS   "There are no usb devices" NM_MSG_ANY_KEY
#define NM_MSG_USB_NONE   "There are no devices attached" NM_MSG_ANY_KEY
#define NM_MSG_Q_CR_ERR   "QMP: cannot create socket" NM_MSG_ANY_KEY
#define NM_MSG_Q_FL_ERR   "QMP: cannot set socket options" NM_MSG_ANY_KEY
#define NM_MSG_Q_CN_ERR   "QMP: cannot connect to socket" NM_MSG_ANY_KEY
#define NM_MSG_Q_SE_ERR   "Error send message to QMP socket" NM_MSG_ANY_KEY
#define NM_MSG_Q_NO_ANS   "QMP: no answer" NM_MSG_ANY_KEY
#define NM_MSG_Q_EXEC_E   "QMP: execute error" NM_MSG_ANY_KEY
#define NM_MSG_START_ERR  "Start failed, error was logged" NM_MSG_ANY_KEY
#define NM_MSG_INC_DEL    "Some files was not deleted!" NM_MSG_ANY_KEY
#define NM_MSG_SOCK_USED  "Socket is already used!" NM_MSG_ANY_KEY
#define NM_MSG_TTY_MISS   "TTY is missing!" NM_MSG_ANY_KEY
#define NM_MSG_TTY_INVAL  "TTY is not terminal!" NM_MSG_ANY_KEY
#define NM_MSG_MTAP_NSET  "MacVTap parent iface is not set" NM_MSG_ANY_KEY
#define NM_MSG_TAP_EACC   "Access to tap iface is missing" NM_MSG_ANY_KEY
#define NM_MSG_NO_SNAPS   "There are no snapshots" NM_MSG_ANY_KEY
#define NM_NSG_DRV_LIM    NM_STRING(NM_DRIVE_LIMIT) " disks limit reached" NM_MSG_ANY_KEY
#define NM_MSG_DRV_NONE   "No additional disks" NM_MSG_ANY_KEY
#define NM_MSG_DRV_EDEL   "Cannot delete drive from filesystem" NM_MSG_ANY_KEY
#define NM_MSG_MAC_INVAL  "Invalid mac address" NM_MSG_ANY_KEY
#define NM_MSG_MAC_USED   "This mac address is already used" NM_MSG_ANY_KEY
#define NM_MSG_VHOST_ERR  "vhost can be enabled only on virtio-net" NM_MSG_ANY_KEY
#define NM_MSG_VTAP_NOP   "MacVTap parent interface does not exists" NM_MSG_ANY_KEY
#define NM_MSG_NAME_DIFF  "Names must be different" NM_MSG_ANY_KEY
#define NM_MSG_OVF_MISS   "OVF file is not found" NM_MSG_ANY_KEY
#define NM_MSG_OVF_EPAR   "Cannot parse OVF file" NM_MSG_ANY_KEY
#define NM_MSG_XPATH_ERR  "Cannot create new XPath context" NM_MSG_ANY_KEY
#define NM_MSG_NS_ERROR   "Cannot register xml namespaces" NM_MSG_ANY_KEY
#define NM_MSG_USB_EMPTY  "Empty device name" NM_MSG_ANY_KEY
#define NM_MSG_USB_EDATA  "Malformed input data" NM_MSG_ANY_KEY
#define NM_MSG_USB_ATTAC  "Already attached" NM_MSG_ANY_KEY

#define NM_ERASE_TITLE(t, cols) \
    mvwhline(t ## _window, 1, 1, ' ', (cols) - 2)

enum nm_color {
    NM_COLOR_BLACK       = 1,
    NM_COLOR_HIGHLIGHT   = 3,
    NM_COLOR_RED         = 4
};

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
    NM_KEY_G = 103,
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
    NM_KEY_A_UP = 65,
    NM_KEY_D_UP = 68,
    NM_KEY_I_UP = 73,
    NM_KEY_N_UP = 78,
    NM_KEY_O_UP = 79,
    NM_KEY_P_UP = 80,
    NM_KEY_R_UP = 82,
    NM_KEY_S_UP = 83,
    NM_KEY_V_UP = 86,
    NM_KEY_X_UP = 88
};

/*
** Fit string in action window.
** ch1 and ch2 need for snapshot tree.
** I dont know how to print ACS_* chars in mvwprintw().
*/
#define NM_PR_VM_INFO()                                         \
    do {                                                        \
        if (y > (rows - 3)) {                                   \
            mvwprintw(action_window, y, x, "...");              \
            return;                                             \
        }                                                       \
        if (ch1 && ch2) {                                       \
            mvwaddch(action_window, y, x, ch1 );                \
            mvwaddch(action_window, y, x + 1, ch2 );            \
        }                                                       \
        nm_align2line(&buf, (ch1 && ch2) ? cols - 2 : cols);    \
        mvwprintw(action_window, y++,                           \
                (ch1 && ch2) ? x + 2 : x, "%s", buf.data);      \
        nm_str_trunc(&buf, 0);                                  \
        ch1 = ch2 = 0;                                          \
    } while (0)

#endif /* NM_WINDOW_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
