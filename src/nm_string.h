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
void nm_str_add_char_opt(nm_str_t *str, char ch);
void nm_str_add_text(nm_str_t *str, const char *src);
void nm_str_add_str(nm_str_t *str, const nm_str_t *src);
void nm_str_copy(nm_str_t *str, const nm_str_t *src);
void nm_str_trunc(nm_str_t *str, size_t len);
void nm_str_free(nm_str_t *str);
void nm_str_dirname(const nm_str_t *str, nm_str_t *res);
void nm_str_basename(const nm_str_t *str, nm_str_t *res);
uint32_t nm_str_stoui(const nm_str_t *str, int base);
uint64_t nm_str_stoul(const nm_str_t *str, int base);
int64_t nm_str_stol(const nm_str_t *str, int base);
uint64_t nm_str_ttoul(const char *str, int base);
int nm_str_cmp_st(const nm_str_t *str, const char *text);
int nm_str_cmp_tt(const char *text1, const char *text2);
int nm_str_cmp_ss(const nm_str_t *str1, const nm_str_t *str2);
int nm_strn_cmp_ss(const nm_str_t *str1, const nm_str_t *str2);
size_t nm_strlcpy(char *dst, const char *src, size_t buflen);
void nm_str_remove_char(nm_str_t *str, char ch);
void nm_str_format(nm_str_t *str, const char *fmt, ...)
    __attribute__ ((format(printf, 2, 3)));

void nm_str_vect_ins_cb(const void *unit_p, const void *ctx);
void nm_str_vect_free_cb(const void *unit_p);

#endif /* NM_STRING_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
