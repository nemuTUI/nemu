#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_machine.h>
#include <nm_cfg_file.h>

#include <sys/wait.h>

static void nm_mach_get(const char *arch);

void nm_mach_init(void)
{
    const nm_vect_t *archs = &nm_cfg_get()->qemu_targets;

    for (size_t n = 0; n < archs->n_memb; n++)
    {
        nm_mach_get(((char **) archs->data)[n]);
    }
}

void nm_mach_free(void)
{
    //...
}

static void nm_mach_get(const char *arch)
{
    int pipefd[2];
    char buf[1000]; /* XXX use heap */
    nm_str_t qemu_bin_path = NM_INIT_STR;
    nm_str_t qemu_bin_name = NM_INIT_STR;

    nm_str_format(&qemu_bin_name, "qemu-system-%s", arch);
    nm_str_format(&qemu_bin_path, "%s/bin/%s",
        NM_STRING(NM_USR_PREFIX), qemu_bin_name.data);

    if (pipe(pipefd) == -1)
        nm_bug("%s: pipe: %s", __func__, strerror(errno));

#ifdef NM_DEBUG
    nm_debug("Get machine list for %s:\n", arch);
#endif

    switch (fork()) {
    case (-1):  /* error*/
        nm_bug("%s: fork: %s", __func__, strerror(errno));
        break;
    
    case (0):   /* child */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execl(qemu_bin_path.data, qemu_bin_name.data, "-M", "help", NULL);
        nm_bug("%s: unreachable reached", __func__);
        break;

    default:    /* parent */
        close(pipefd[1]);
        read(pipefd[0], buf, sizeof(buf));
#ifdef NM_DEBUG
        nm_debug("%s\n", buf);
#endif
        wait(NULL);
        break;
    }

    nm_str_free(&qemu_bin_path);
    nm_str_free(&qemu_bin_name);
}

/* vim:set ts=4 sw=4 fdm=marker: */
