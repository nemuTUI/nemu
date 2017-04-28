#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_machine.h>
#include <nm_cfg_file.h>

#include <sys/wait.h>

#define NM_INIT_MLIST { NM_INIT_STR, NULL }
#define NM_PIPE_READLEN 4096

#define nm_mach_arch(p) ((nm_mach_t *) p)->arch
#define nm_mach_list(p) ((nm_mach_t *) p)->list

static nm_vect_t nm_machs = NM_INIT_VECT;

static void nm_mach_init(void);
static void nm_mach_get_data(const char *arch);
static nm_vect_t *nm_mach_parse(const char *buf);

static void nm_mach_init(void)
{
    const nm_vect_t *archs = &nm_cfg_get()->qemu_targets;

    for (size_t n = 0; n < archs->n_memb; n++)
        nm_mach_get_data(((char **) archs->data)[n]);

#ifdef NM_DEBUG
    nm_debug("\n");
    for (size_t n = 0; n < nm_machs.n_memb; n++)
    {
        nm_vect_t *v = nm_mach_list(nm_machs.data[n]);
        nm_debug("Get machine list for %s:\n",
            nm_mach_arch(nm_machs.data[n]).data);

        for (size_t n = 0; n < v->n_memb; n++)
        {
            nm_debug(">> mach: %s\n", (char *) v->data[n]);
        }
    }
#endif
}

const char **nm_mach_get(const nm_str_t *arch)
{
    const char **v = NULL;

    if (nm_machs.data == NULL)
        nm_mach_init();

    for (size_t n = 0; n < nm_machs.n_memb; n++)
    {
        if (nm_str_cmp_ss(nm_machs.data[n], arch) == NM_OK)
        {
            v = (const char **) nm_mach_list(nm_machs.data[n])->data;
            break;
        }
    }

    return v;
}

void nm_mach_free(void)
{
    for (size_t n = 0; n < nm_machs.n_memb; n++)
        nm_vect_free(nm_mach_list(nm_machs.data[n]), NULL);

    nm_vect_free(&nm_machs, nm_mach_vect_free_mlist_cb);
}

static void nm_mach_get_data(const char *arch)
{
    int pipefd[2];
    char *buf = NULL;
    nm_str_t qemu_bin_path = NM_INIT_STR;
    nm_str_t qemu_bin_name = NM_INIT_STR;
    nm_mach_t mach_list = NM_INIT_MLIST;

    nm_str_alloc_text(&mach_list.arch, arch);

    nm_str_format(&qemu_bin_name, "qemu-system-%s", arch);
    nm_str_format(&qemu_bin_path, "%s/bin/%s",
        NM_STRING(NM_USR_PREFIX), qemu_bin_name.data);

    if (pipe(pipefd) == -1)
        nm_bug("%s: pipe: %s", __func__, strerror(errno));

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
            mach_list.list = nm_mach_parse(buf);
            wait(NULL);
        }
        break;
    }

    nm_vect_insert(&nm_machs, &mach_list,
        sizeof(mach_list), nm_mach_vect_ins_mlist_cb);

    nm_str_free(&qemu_bin_path);
    nm_str_free(&qemu_bin_name);
    nm_str_free(&mach_list.arch);
    free(buf);
}

static nm_vect_t *nm_mach_parse(const char *buf)
{
    const char *bufp = buf;
    int lookup_mach = 1;
    nm_str_t mach = NM_INIT_STR;
    nm_vect_t *v = NULL;

    v = nm_calloc(1, sizeof(nm_vect_t));

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

        if (!lookup_mach && (*bufp == '\n'))
        {
            nm_str_t item = NM_INIT_STR;

            nm_str_copy(&item, &mach);
            nm_vect_insert(v, item.data, item.len + 1, NULL);
            nm_str_trunc(&mach, 0);
            lookup_mach = 1;

            nm_str_free(&item);
        }

        bufp++;
    }

    nm_vect_end_zero(v);

    nm_str_free(&mach);

    return v;
}

void nm_mach_vect_ins_mlist_cb(const void *unit_p, const void *ctx)
{
    nm_str_copy(&nm_mach_arch(unit_p), &nm_mach_arch(ctx));
    memcpy(&nm_mach_list(unit_p), &nm_mach_list(ctx), sizeof(nm_vect_t *));
}

void nm_mach_vect_free_mlist_cb(const void *unit_p)
{
    nm_str_free(&nm_mach_arch(unit_p));
    free(nm_mach_list(unit_p));
}

/* vim:set ts=4 sw=4 fdm=marker: */
