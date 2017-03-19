#ifndef NM_STRING_H_
#define NM_STRING_H_

typedef struct {
    char *data;
    size_t len;
    size_t alloc_bytes;
} nm_str_t;

#define NM_INIT_STR { NULL, 0, 0 }

void nm_str_alloc_text(nm_str_t *str, const char *src);
void nm_str_alloc_str(nm_str_t *str, const nm_str_t *src);
void nm_str_add_char(nm_str_t *str, char ch);
void nm_str_add_text(nm_str_t *str, const char *src);
void nm_str_add_str(nm_str_t *str, const nm_str_t *src);
void nm_str_copy(nm_str_t *str, const nm_str_t *src);
void nm_str_free(nm_str_t *str);

#endif /* NM_STRING_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
