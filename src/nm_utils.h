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

typedef struct {
    size_t smp;
    size_t sockets;
    size_t cores;
    size_t threads;
} nm_cpu_t;

#define NM_INIT_FILE (nm_file_map_t) { NULL, -1, 0, NULL }
#define NM_INIT_CPU (nm_cpu_t) { 0, 0, 0, 0 }

void *nm_alloc(size_t size);
void *nm_calloc(size_t nmemb, size_t size);
void *nm_realloc(void *p, size_t size);
void nm_map_file(nm_file_map_t *file);
void nm_copy_file(const nm_str_t *src, const nm_str_t *dst);
void nm_unmap_file(const nm_file_map_t *file);
/* Execute process. Read stdout if answer is not NULL */
int nm_spawn_process(const nm_vect_t *argv, nm_str_t *answer);

void nm_bug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)))
    __attribute__((noreturn));

void nm_debug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));

void nm_cmd_str(nm_str_t *str, const nm_vect_t *argv);
void nm_parse_smp(nm_cpu_t *cpu, const char *src);

void nm_exit(int status)
    __attribute__((noreturn));

int nm_mkdir_parent(const nm_str_t *path, mode_t mode);

const char *nm_nemu_path();

/*
 * get date and time, format:
 *
 * %Y - year
 * %m - month
 * %d - day of the month
 * %H - hours
 * %M - minutes
 * %S - seconds
 * %% - '%'
 */
void nm_get_time(nm_str_t *res, const char *fmt);
void nm_gen_rand_str(nm_str_t *res, size_t len);
void nm_gen_uid(nm_str_t *res);

#endif /* NM_UTILS_H_ */
/* vim:set ts=4 sw=4: */
