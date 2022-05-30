#ifndef NM_FTW_H_
#define NM_FTW_H_

#include <nm_string.h>
#include <sys/stat.h>

/*
 * FTW - file tree walk.
 * This is a clone of the nftw(3) function.
 * I don't want to use _XOPEN_SOURCE macro.
 * All logic is taken from musl (src/misc/nftw.c).
 */

enum nm_ftw_flags {
    NM_FTW_DNFSL = (1 << 0), /* do not follow symbolic links    */
    NM_FTW_MOUNT = (1 << 1), /* stay within the same filesystem */
    NM_FTW_CHDIR = (1 << 2), /* do a chdir(2) to each directory */
    NM_FTW_DEPTH = (1 << 3)  /* do a post-order traversal       */
};

enum nm_ftw_type {
    NM_FTW_REG = 1, /* path is a regular file                                 */
    NM_FTW_DIR, /* path is a directory                                        */
    NM_FTW_DRE, /* path is a directory which can't be read                    */
    NM_FTW_NS,  /* stat(2) call failed on path, which is not a symbolic link  */
    NM_FTW_SL,  /* path is a symbolic link, and NM_FTW_DNFSL was set in flags */
    NM_FTW_DP,  /* path is a directory, and FTW_DEPTH was specified in flags  */
    NM_FTW_SLN  /* path is a symbolic link pointing to a nonexistent file     */
};

#define NM_FTW_DEPTH_UNLIM -1

typedef struct {
    int base;
    int level;
} nm_ftw_t;

typedef int (*nm_ftw_cb_t)(const char *path, const struct stat *st,
        enum nm_ftw_type type, nm_ftw_t *ftw, void *ctx);

int nm_ftw(const nm_str_t *path, nm_ftw_cb_t cb, void *ctx,
        int max_depth, enum nm_ftw_flags flags);

#endif /* NM_FTW_H_ */
/* vim:set ts=4 sw=4: */
