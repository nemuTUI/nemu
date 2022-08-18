#include <nm_core.h>
#include <nm_ftw.h>

#include <dirent.h>

typedef struct nm_history {
    struct nm_history *chain;
    dev_t dev;
    ino_t ino;
    int level;
    int base;
} nm_history_t;

static int nm_do_ftw(char *path, nm_ftw_cb_t cb, void *ctx, int max_depth,
        enum nm_ftw_flags flags, nm_history_t *history)
{
    size_t path_len = strlen(path);
    int rc, dfd = -1, err = 0;
    nm_history_t history_new;
    enum nm_ftw_type type;
    size_t base_len;
    struct stat st;
    nm_ftw_t ftw;

    if ((flags & NM_FTW_DNFSL) ?
            lstat(path, &st) : stat(path, &st) < 0) {
        if (!(flags & NM_FTW_DNFSL) &&
                errno == ENOENT && lstat(path, &st)) {
            type = NM_FTW_SLN;
        } else if (errno != EACCES) {
            return NM_ERR;
        } else {
            type = NM_FTW_NS;
        }
    } else if (S_ISDIR(st.st_mode)) {
        if (flags & NM_FTW_DEPTH) {
            type = NM_FTW_DP;
        } else {
            type = NM_FTW_DIR;
        }
    } else if (S_ISLNK(st.st_mode)) {
        if (flags & NM_FTW_DNFSL) {
            type = NM_FTW_SL;
        } else {
            type = NM_FTW_SLN;
        }
    } else {
        type = NM_FTW_REG;
    }

    if ((flags & NM_FTW_MOUNT) && history && st.st_dev != history->dev) {
        return NM_OK;
    }

    base_len = (path_len && path[path_len - 1] == '/') ?
        path_len - 1 : path_len;

    history_new.chain = history;
    history_new.dev = st.st_dev;
    history_new.ino = st.st_ino;
    history_new.level = (history) ? history->level + 1 : 0;
    history_new.base = base_len + 1;

    ftw.level = history_new.level;
    if (history) {
        ftw.base = history->base;
    } else {
        size_t n = base_len;

        for(; n && path[n] == '/'; n--);
        for(; n && path[n] != '/'; n--);
        ftw.base = n;
    }

    if (type == NM_FTW_DIR || type == NM_FTW_DP) {
        dfd = open(path, O_RDONLY);
        err = errno;

        if (dfd < 0 && err == EACCES) {
            type = NM_FTW_DRE;
        }

        if (!max_depth) {
            close(dfd);
        }
    }

    if (!(flags & NM_FTW_DEPTH) &&
            (rc = cb(path, &st, type, &ftw, ctx)) != NM_OK) {
        return rc;
    }

    for (; history; history = history->chain) {
        if (history->dev == st.st_dev && history->ino == st.st_ino) {
            return NM_OK;
        }
    }

    if ((type == NM_FTW_DIR || type == NM_FTW_DP) && max_depth) {
        DIR *d;

        if (dfd < 0 && err != 0) {
            errno = err;
            return NM_ERR;
        }

        if ((d = fdopendir(dfd))) {
            struct dirent *de;

            while ((de = readdir(d))) {
                if (de->d_name[0] == '.' && (!de->d_name[1] ||
                            (de->d_name[1] == '.' && !de->d_name[2]))) {
                    continue;
                }

                if (strlen(de->d_name) >= PATH_MAX - path_len) {
                    errno = ENAMETOOLONG;
                    closedir(d);
                    return NM_ERR;
                }

                path[base_len] = '/';
                strcpy(path + base_len + 1, de->d_name);

                if (max_depth != NM_FTW_DEPTH_UNLIM) {
                    max_depth--;
                }

                if ((rc = nm_do_ftw(path, cb, ctx, max_depth,
                                flags, &history_new)) != NM_OK) {
                    closedir(d);
                    return rc;
                }
            }
            closedir(d);
        } else {
            close(dfd);
            return NM_ERR;
        }
    }

    path[base_len] = '\0';

    if ((flags & NM_FTW_DEPTH) &&
            (rc = cb(path, &st, type, &ftw, ctx)) != NM_OK) {
        return rc;
    }

    return NM_OK;
}

int nm_ftw(const nm_str_t *path, nm_ftw_cb_t cb,
        void *ctx, int max_depth, enum nm_ftw_flags flags)
{
    int rc = NM_ERR;
    char path_buf[PATH_MAX + 1] = {0};

    if (path->len > PATH_MAX) {
        errno = ENAMETOOLONG;
        return NM_ERR;
    }

    memcpy(path_buf, path->data, path->len);

    rc = nm_do_ftw(path_buf, cb, ctx, max_depth, flags, NULL);

    return rc;
}
/* vim:set ts=4 sw=4: */
