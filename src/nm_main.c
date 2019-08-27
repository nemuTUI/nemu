#include <nm_core.h>
#include <nm_menu.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_main_loop.h>
#include <nm_vm_control.h>
#include <nm_lan_settings.h>

#if defined (NM_OS_LINUX)
  #define NM_OPT_ARGS "cs:vhl"
#else
  #define NM_OPT_ARGS "s:vhl"
#endif

static void signals_handler(int signal);
static void nm_process_args(int argc, char **argv);
static void nm_print_feset(void);

volatile sig_atomic_t redraw_window = 0;

nm_window_t *help_window;
nm_window_t *side_window;
nm_window_t *action_window;

int main(int argc, char **argv)
{
    struct sigaction sa;

    setlocale(LC_ALL,"");
    bindtextdomain(NM_PROGNAME, NM_LOCALE_PATH);
    textdomain(NM_PROGNAME);

    nm_process_args(argc, argv);

    nm_cfg_init();
    nm_db_init();
#if defined (NM_OS_LINUX)
    nm_lan_create_veth(NM_FALSE);
#endif

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signals_handler;
    sigaction(SIGWINCH, &sa, NULL);

    nm_ncurses_init();

    nm_start_main_loop();

    return NM_OK;
}

static void signals_handler(int signal)
{
    switch (signal) {
    case SIGWINCH:
        redraw_window = 1;
        break;
    }
}

static void nm_process_args(int argc, char **argv)
{
    int opt;
    const char *optstr = NM_OPT_ARGS;
    nm_str_t vmname = NM_INIT_STR;
    nm_vect_t vm_list = NM_INIT_VECT;

    static const struct option longopts[] = {
#if defined (NM_OS_LINUX)
        { "create-veth", no_argument,       NULL, 'c' },
#endif
        { "start",       required_argument, NULL, 's' },
        { "list",        no_argument,       NULL, 'l' },
        { "version",     no_argument,       NULL, 'v' },
        { "help",        no_argument,       NULL, 'h' },
        { NULL,          0,                 NULL,  0  }
    };

    while ((opt = getopt_long(argc, argv, optstr, longopts, NULL)) != -1)
    {
        switch (opt) {
#if defined (NM_OS_LINUX)
        case 'c':
            nm_init_core();
            nm_lan_create_veth(NM_TRUE);
            nm_exit_core();
#endif
        case 's':
            nm_init_core();
            nm_str_alloc_text(&vmname, optarg);
            nm_vmctl_start(&vmname, 0);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'l':
            nm_init_core();
            nm_db_select(NM_GET_VMS_SQL, &vm_list);
            for (size_t i = 0; i < vm_list.n_memb; ++i)
                printf("%s\n", ((nm_str_t*)vm_list.data[i])->data);
            nm_vect_free(&vm_list, nm_str_vect_free_cb);
            nm_exit_core();
        case 'v':
            printf("nEMU %s\n", NM_VERSION);
            nm_print_feset();
            exit(NM_OK);
        case 'h':
#if defined (NM_OS_LINUX)
            printf("%s\n", _("-c, --create-veth   create veth interfaces"));
#endif
            printf("%s\n", _("-s, --start <name>  start vm"));
            printf("%s\n", _("-l, --list          list vms"));
            printf("%s\n", _("-v, --version       show version"));
            printf("%s\n", _("-h, --help          show help"));
            exit(NM_OK);
        default:
            exit(NM_ERR);
        }
    }
}

static void nm_print_feset(void)
{
    nm_vect_t feset = NM_INIT_VECT;
    nm_str_t msg = NM_INIT_STR;

#if defined (NM_WITH_SPICE)
    nm_vect_insert_cstr(&feset, "+ spice");
#endif
#if defined (NM_WITH_VNC_CLIENT)
    nm_vect_insert_cstr(&feset, "+ vnc-client");
#endif
#if defined (NM_SAVEVM_SNAPSHOTS)
    nm_vect_insert_cstr(&feset, "+ savevm");
#endif
#if defined (NM_WITH_OVF_SUPPORT)
    nm_vect_insert_cstr(&feset, "+ ovf");
#endif
#if defined (NM_WITH_NETWORK_MAP)
    nm_vect_insert_cstr(&feset, "+ svg");
#endif
#if defined (NM_DEBUG)
    nm_vect_insert_cstr(&feset, "+ debug");
#endif

    for (size_t n = 0; n < feset.n_memb; n++)
        nm_str_append_format(&msg, " %s\n", (char *) nm_vect_at(&feset, n));

    if (msg.len)
        printf("%s", msg.data);

    nm_vect_free(&feset, NULL);
    nm_str_free(&msg);
}
/* vim:set ts=4 sw=4 fdm=marker: */
