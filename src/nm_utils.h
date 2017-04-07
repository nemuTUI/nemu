#ifndef NM_UTILS_H_
#define NM_UTILS_H_

#include <nm_string.h>

typedef struct {
    const nm_str_t *name;
    int fd;
    off_t size;
    void *mp;
} nm_file_map_t;

void *nm_alloc(size_t size);
void *nm_calloc(size_t nmemb, size_t size);
void *nm_realloc(void *p, size_t size);
void nm_map_file(nm_file_map_t *file);
void nm_copy_file(const nm_str_t *src, const nm_str_t *dst);
void nm_unmap_file(const nm_file_map_t *file);

void nm_bug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));

#ifdef NM_DEBUG
void nm_debug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));
#endif

#define NM_INIT_FILE { NULL, -1, 0, NULL }

#endif /* NM_UTILS_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
