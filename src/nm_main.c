#include <nm_core.h>
#include <nm_utils.h>
#include <nm_menu.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_main_loop.h>
#include <nm_mon_daemon.h>
#include <nm_ovf_import.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>
#include <nm_lan_settings.h>

#if defined (NM_OS_LINUX)
    static const char NM_OPT_ARGS[] = "cs:p:f:z:k:i:m:C:vhld";
#else
    static const char NM_OPT_ARGS[] = "s:p:f:z:k:i:m:C:vhld";
#endif

static void signals_handler(int signal);
static void nm_process_args(int argc, char **argv);
static void nm_print_feset(void);

volatile sig_atomic_t redraw_window = 0;

nm_window_t *help_window;
nm_window_t *side_window;
nm_window_t *action_window;

nm_panel_t *help_panel;
nm_panel_t *side_panel;
nm_panel_t *action_panel;

int main(int argc, char **argv)
{
    struct sigaction sa;

    setlocale(LC_ALL,"");
    bindtextdomain(NM_PROGNAME, NM_FULL_LOCALEDIR);
    textdomain(NM_PROGNAME);

    nm_process_args(argc, argv);

    nm_cfg_init();
    nm_db_init();
    nm_mon_start();
#if defined (NM_OS_LINUX)
    nm_lan_create_veth(NM_FALSE);
#endif
#if defined (NM_WITH_OVF_SUPPORT)
    nm_ovf_init();
#endif

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signals_handler;
    sigaction(SIGWINCH, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

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
    case SIGINT:
        nm_exit(EXIT_SUCCESS);
    case SIGTERM:
        nm_exit(SIGTERM);
    }
}

static void nm_process_args(int argc, char **argv)
{
    int opt;
    const char *optstr = NM_OPT_ARGS;
    nm_str_t vmname = NM_INIT_STR;
    nm_str_t vmnames = NM_INIT_STR;
    nm_vect_t vm_list = NM_INIT_VECT;

    static const struct option longopts[] = {
#if defined (NM_OS_LINUX)
        { "create-veth", no_argument,       NULL, 'c' },
#endif
        { "start",       required_argument, NULL, 's' },
        { "powerdown",   required_argument, NULL, 'p' },
        { "force-stop",  required_argument, NULL, 'f' },
        { "reset",       required_argument, NULL, 'z' },
        { "kill",        required_argument, NULL, 'k' },
        { "info",        required_argument, NULL, 'i' },
        { "cmd",         required_argument, NULL, 'm' },
        { "cfg",         required_argument, NULL, 'C' },
        { "list",        no_argument,       NULL, 'l' },
        { "daemon",      no_argument,       NULL, 'd' },
        { "version",     no_argument,       NULL, 'v' },
        { "help",        no_argument,       NULL, 'h' },
        { NULL,          0,                 NULL,  0  }
    };

    while ((opt = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
        switch (opt) {
#if defined (NM_OS_LINUX)
        case 'c':
            nm_init_core();
            nm_lan_create_veth(NM_TRUE);
            nm_exit_core();
#endif
        case 's':
            nm_init_core();

            nm_str_alloc_text(&vmnames, optarg);
            nm_str_append_to_vect(&vmnames, &vm_list, ",");

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_str_alloc_text(&vmname, vm_list.data[n]);
                if (nm_qmp_test_socket(&vmname) != NM_OK)
                    nm_vmctl_start(&vmname, 0);
            }

            nm_str_free(&vmnames);
            nm_vect_free(&vm_list, NULL);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'p':
            nm_init_core();

            nm_str_alloc_text(&vmnames, optarg);
            nm_str_append_to_vect(&vmnames, &vm_list, ",");

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_str_alloc_text(&vmname, vm_list.data[n]);
                nm_qmp_vm_shut(&vmname);
            }

            nm_str_free(&vmnames);
            nm_vect_free(&vm_list, NULL);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'f':
            nm_init_core();

            nm_str_alloc_text(&vmnames, optarg);
            nm_str_append_to_vect(&vmnames, &vm_list, ",");

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_str_alloc_text(&vmname, vm_list.data[n]);
                nm_qmp_vm_stop(&vmname);
            }

            nm_str_free(&vmnames);
            nm_vect_free(&vm_list, NULL);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'z':
            nm_init_core();

            nm_str_alloc_text(&vmnames, optarg);
            nm_str_append_to_vect(&vmnames, &vm_list, ",");

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_str_alloc_text(&vmname, vm_list.data[n]);
                nm_qmp_vm_reset(&vmname);
            }

            nm_str_free(&vmnames);
            nm_vect_free(&vm_list, NULL);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'k':
            nm_init_core();

            nm_str_alloc_text(&vmnames, optarg);
            nm_str_append_to_vect(&vmnames, &vm_list, ",");

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_str_alloc_text(&vmname, vm_list.data[n]);
                nm_vmctl_kill(&vmname);
            }

            nm_str_free(&vmnames);
            nm_vect_free(&vm_list, NULL);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'i':
            nm_init_core();

            nm_str_alloc_text(&vmnames, optarg);
            nm_str_append_to_vect(&vmnames, &vm_list, ",");

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_str_alloc_text(&vmname, vm_list.data[n]);
                nm_str_t info = nm_vmctl_info(&vmname);
                printf(n < vm_list.n_memb - 1 ? "%s\n" : "%s", info.data);
                nm_str_free(&info);
            }

            nm_str_free(&vmnames);
            nm_vect_free(&vm_list, NULL);
            nm_str_free(&vmname);
            nm_exit_core();
        case 'm':
            nm_init_core();
            {
                nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
                nm_vect_t _argv = NM_INIT_VECT;
                nm_str_t name = NM_INIT_STR;
                int flags = 0;

                flags |= NM_VMCTL_INFO;
                nm_str_format(&name, "%s", optarg);
                nm_vmctl_get_data(&name, &vm);
                nm_vmctl_gen_cmd(&_argv, &vm, &name, &flags, NULL, NULL);

                for (size_t n = 0; n < _argv.n_memb; n++) {
                    printf("%s%s", (char *) nm_vect_at(&_argv, n),
                            (n != _argv.n_memb - 1) ? " " : "");
                }

                nm_str_free(&name);
                nm_vect_free(&_argv, NULL);
                nm_vmctl_free_data(&vm);
            }
            nm_exit_core();
        case 'C':
            nm_cfg_path = optarg;
            break;
        case 'd':
            nm_mon_loop();
            nm_cfg_free();
            nm_exit(NM_OK);
        case 'l':
            nm_init_core();
            nm_db_select(NM_GET_VMS_SQL, &vm_list);
            for (size_t i = 0; i < vm_list.n_memb; ++i) {
                const nm_str_t *name = (nm_str_t *)vm_list.data[i];
                int status = nm_qmp_test_socket(name);
                printf("%s - %s\n", name->data, status == NM_OK ? "running" : "stopped");
            }
            nm_vect_free(&vm_list, nm_str_vect_free_cb);
            nm_exit_core();
        case 'v':
            nm_cfg_init();
            printf("nEMU %s\n", NM_VERSION);
            nm_print_feset();
            nm_cfg_free();
            nm_exit(NM_OK);
        case 'h':
            printf("%s\n", _("-s, --start      <name> start vm"));
            printf("%s\n", _("-p, --powerdown  <name> powerdown vm"));
            printf("%s\n", _("-f, --force-stop <name> shutdown vm"));
            printf("%s\n", _("-z, --reset      <name> reset vm"));
            printf("%s\n", _("-k, --kill       <name> kill vm process"));
            printf("%s\n", _("-i, --info       <name> print vm info"));
            printf("%s\n", _("-m, --cmd        <name> print vm command line"));
            printf("%s\n", _("-C, --cfg        <path> path to config file"));
            printf("%s\n", _("-l, --list              list vms"));
            printf("%s\n", _("-d, --daemon            vm monitoring daemon"));
#if defined (NM_OS_LINUX)
            printf("%s\n", _("-c, --create-veth       create veth interfaces"));
#endif
            printf("%s\n", _("-v, --version           show version"));
            printf("%s\n", _("-h, --help              show help"));
            nm_exit(NM_OK);
        default:
            nm_exit(NM_ERR);
        }
    }
}

#define NM_FESET(feset, _g_, _c_) \
    (cfg->glyphs.checkbox) ? NM_GLYPH_CK_##_g_ feset : _c_ feset

static void nm_print_feset(void)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    nm_vect_t feset = NM_INIT_VECT;
    nm_str_t msg = NM_INIT_STR;

    nm_vect_insert_cstr(&feset,
#if defined (NM_WITH_OVF_SUPPORT)
            NM_FESET(" OVF import", YES, "+")
#else
            NM_FESET(" OVF import", NO, "-")
#endif
            );
    nm_vect_insert_cstr(&feset,
#if defined (NM_WITH_NETWORK_MAP)
            NM_FESET(" SVG map", YES, "+")
#else
            NM_FESET(" SVG map", NO, "-")
#endif
            );
    nm_vect_insert_cstr(&feset,
#if defined (NM_WITH_NEWLINKPROP)
            NM_FESET(" Link alt names", YES, "+")
#else
            NM_FESET(" Link alt names", NO, "-")
#endif
            );
    nm_vect_insert_cstr(&feset,
#if defined (NM_WITH_DBUS)
            NM_FESET(" D-Bus support", YES, "+")
#else
            NM_FESET(" D-Bus support", NO, "-")
#endif
            );
    nm_vect_insert_cstr(&feset,
#if defined (NM_WITH_REMOTE)
            NM_FESET(" Remote control", YES, "+")
#else
            NM_FESET(" Remote control", NO, "-")
#endif
            );

    for (size_t n = 0; n < feset.n_memb; n++)
        nm_str_append_format(&msg, " %s\n", (char *) nm_vect_at(&feset, n));

    if (msg.len)
        printf("%s", msg.data);

    nm_vect_free(&feset, NULL);
    nm_str_free(&msg);
}
/* vim:set ts=4 sw=4: */
