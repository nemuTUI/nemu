#ifndef NM_UTILS_H_
#define NM_UTILS_H_

#include <nm_string.h>
#include <nm_vector.h>

typedef struct {
    const nm_str_t *name;
    int fd;
    off_t size;
    void *mp;
} nm_file_map_t;

#define NM_INIT_FILE (nm_file_map_t) { NULL, -1, 0, NULL }

void *nm_alloc(size_t size);
void *nm_calloc(size_t nmemb, size_t size);
void *nm_realloc(void *p, size_t size);
void nm_map_file(nm_file_map_t *file);
void nm_copy_file(const nm_str_t *src, const nm_str_t *dst);
void nm_unmap_file(const nm_file_map_t *file);
/* Execute process. Read stdout if answer is not NULL */
int nm_spawn_process(const nm_vect_t *argv, nm_str_t *answer);
#if NM_WITH_NOTIFY
void nm_send_notify(const nm_str_t *msg);
#endif

void nm_bug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));

void nm_debug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));

void nm_cmd_str(nm_str_t *str, const nm_vect_t *argv);

#endif /* NM_UTILS_H_ */
/* vim:set ts=4 sw=4: */
