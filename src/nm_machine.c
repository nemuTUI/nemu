#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_machine.h>
#include <nm_cfg_file.h>

#include <sys/wait.h>

#define NM_PIPE_READLEN 4096

static void nm_mach_get(const char *arch);
static void nm_mach_parse(const char *buf);

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
    char *buf = NULL;
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
        {
            char *bufp = NULL;
            ssize_t nread;
            size_t nalloc = NM_PIPE_READLEN * 2, total_read = 0;
            close(pipefd[1]);

            buf = nm_alloc(NM_PIPE_READLEN * 2);
            bufp = buf;
            while ((nread = read(pipefd[0], bufp, NM_PIPE_READLEN)) > 0)
            {
                total_read += nread;
                bufp += nread;
                if (total_read >= nalloc)
                {
                    nalloc *= 2;
                    buf = nm_realloc(buf, nalloc);
                    bufp = buf;
                    bufp += total_read;
                }
            }
            buf[total_read] = '\0';
            nm_mach_parse(buf);
            wait(NULL);
        }
        break;
    }

    nm_str_free(&qemu_bin_path);
    nm_str_free(&qemu_bin_name);
    free(buf);
}

static void nm_mach_parse(const char *buf)
{
    const char *bufp = buf;
    int lookup_mach = 1;
    int lookup_desc = 0;
    nm_str_t mach = NM_INIT_STR;
    nm_str_t desc = NM_INIT_STR;

    /* skip first line */
    while (*bufp != '\n')
        bufp++;

    bufp++;

    while (*bufp != '\0')
    {
        if (lookup_mach && (*bufp != 0x20))
        {
            nm_str_add_char_opt(&mach, *bufp);
        }
        else if (lookup_mach && (*bufp == 0x20))
        {
            lookup_mach = 0;
        }

        if (!lookup_mach && !lookup_desc && (*bufp != 0x20))
        {
            lookup_desc = 1;
            nm_str_add_char_opt(&desc, *bufp);
        }
        else if (lookup_desc && (*bufp != '\n'))
        {
            nm_str_add_char_opt(&desc, *bufp);
        }
        else if (lookup_desc && (*bufp == '\n'))
        {
#ifdef NM_DEBUG
            nm_debug(">> mach: %s\n", mach.data);
            nm_debug(">> desc: %s\n\n", desc.data);
#endif
            nm_str_trunc(&mach, 0);
            nm_str_trunc(&desc, 0);
            lookup_desc = 0;
            lookup_mach = 1;
        }

        bufp++;
    }

    nm_str_free(&mach);
    nm_str_free(&desc);
}

/* vim:set ts=4 sw=4 fdm=marker: */
