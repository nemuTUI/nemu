#include <nm_core.h>
#include <nm_string.h>
#include <nm_stat_usage.h>


enum {
    NM_STAT_BUF_LEN = 512,
    NM_STAT_RES_LEN = 12,
};

static const char NM_STAT_PATH[] = "/proc/stat";

uint64_t nm_total_cpu_before;
uint64_t nm_total_cpu_after;
uint64_t nm_proc_cpu_before;
uint64_t nm_proc_cpu_after;
uint8_t  nm_cpu_iter;
float    nm_cpu_usage;

static void nm_stat_cpu_total_time(void)
{
    int fd, space_num = 0;
    char buf[NM_STAT_BUF_LEN] = {0};
    char buf_time[NM_STAT_RES_LEN] = {0};
    size_t nread = 0;
    char ch;
    char *token_b = NULL, *token_e = NULL;
    uint64_t res = 0;

    if ((fd = open(NM_STAT_PATH, O_RDONLY)) == -1)
        return;

    while ((read(fd, &ch, 1) > 0) && ch != '\n')
    {
        buf[nread] = ch;
        nread++;
    }

    for (size_t n = 0; n < nread; n++)
    {
        if (buf[n] == 0x20)
            space_num++;
        else
            continue;

        if (space_num == 2)
        {
            token_b = buf + n + 1;
        }

        if (space_num > 2)
        {
            token_e = buf + n;
            memcpy(buf_time, token_b, token_e - token_b);
            res += nm_str_ttoul(buf_time, 10);
            token_b = token_e + 1;
            memset(buf_time, 0, sizeof(buf_time));
        }

        if (space_num == 6)
            break;
    }

    if (nm_cpu_iter)
        nm_total_cpu_after = res;
    else
        nm_total_cpu_before = res;

    close(fd);
}

static void nm_stat_cpu_proc_time(int pid)
{
    int fd, space_num = 0;
    ssize_t nread;
    char buf[NM_STAT_BUF_LEN] = {0};
    char buf_time[NM_STAT_RES_LEN] = {0};
    char *token_b = NULL, *token_e = NULL;
    uint64_t utime = 0, stime = 0;
    nm_str_t path = NM_INIT_STR;

    nm_str_format(&path, "/proc/%d/stat", pid);

    if ((fd = open(path.data, O_RDONLY)) == -1)
        goto out;

    if ((nread = read(fd, buf, sizeof(buf))) <= 0)
        goto out;

    buf[nread - 1] = '\0';

    for (ssize_t n = 0; n < nread; n++)
    {
        if (buf[n] == 0x20)
            space_num++;
        else
            continue;

        if (space_num == 13)
        {
            token_b = buf + n + 1;
        }

        if (space_num == 14)
        {
            token_e = buf + n;
            memcpy(buf_time, token_b, token_e - token_b);
            utime = nm_str_ttoul(buf_time, 10);
            token_b = token_e + 1;
            memset(buf_time, 0, sizeof(buf_time));
        }

        if (space_num == 15)
        {
            token_e = buf + n;
            memcpy(buf_time, token_b, token_e - token_b);
            stime = nm_str_ttoul(buf_time, 10);
            break;
        }
    }

    close(fd);

    if (nm_cpu_iter)
        nm_proc_cpu_after = utime + stime;
    else
        nm_proc_cpu_before = utime + stime;

out:
    nm_str_free(&path);
}

float nm_stat_get_usage(int pid)
{
    float pa, pb, ta, tb;

    nm_stat_cpu_total_time();
    nm_stat_cpu_proc_time(pid);

    pa = nm_proc_cpu_after;
    pb = nm_proc_cpu_before;
    ta = nm_total_cpu_after;
    tb = nm_total_cpu_before;

    if (nm_cpu_iter)
        nm_cpu_usage = ((pa - pb) / (ta - tb)) * 100 * sysconf(_SC_NPROCESSORS_ONLN);

    nm_cpu_iter ^= 1;

    return nm_cpu_usage;
}

/* vim:set ts=4 sw=4 fdm=marker: */
