#ifndef NM_UTILS_H_
#define NM_UTILS_H_

void *nm_alloc(size_t size);
void *nm_realloc(void *p, size_t size);

void nm_bug(const char *fmt, ...)
    __attribute__ ((format(printf, 1, 2)));

#endif /* NM_UTILS_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
