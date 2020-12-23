#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>

static void nm_str_alloc_mem(nm_str_t *str, const char *src, size_t len);
static void nm_str_append_mem(nm_str_t *str, const char *src, size_t len);
static void nm_str_append_mem_opt(nm_str_t *str, const char *src, size_t len);
static const char *nm_str_get(const nm_str_t *str);
static uint64_t nm_str_stoul__(const char *str, int base);

void nm_str_alloc_text(nm_str_t *str, const char *src)
{
    size_t len = strlen(src);
    nm_str_alloc_mem(str, src, len);
}

void nm_str_add_char(nm_str_t *str, char ch)
{
    nm_str_append_mem(str, &ch, sizeof(ch));
}

void nm_str_add_char_opt(nm_str_t *str, char ch)
{
    nm_str_append_mem_opt(str, &ch, sizeof(ch));
}

void nm_str_add_text(nm_str_t *str, const char *src)
{
    size_t len = strlen(src);
    nm_str_append_mem(str, src, len);
}

void nm_str_add_text_part(nm_str_t *str, const char *src, size_t len)
{
    if (!src)
        return;
    nm_str_append_mem(str, src, len);
}

void nm_str_add_str(nm_str_t *str, const nm_str_t *src)
{
    if (!src)
        return;
    nm_str_append_mem(str, src->data, src->len);
}

void nm_str_add_str_part(nm_str_t *str, const nm_str_t *src, size_t len)
{
    if (!src)
        return;
    nm_str_append_mem(str, src->data, len);
}

void nm_str_copy(nm_str_t *str, const nm_str_t *src)
{
    if (!src)
        return;
    nm_str_alloc_mem(str, src->data, src->len);
}

void nm_str_trunc(nm_str_t *str, size_t len)
{
    if (!str)
        return;

    if (len >= str->alloc_bytes)
        nm_bug(_("%s: bad length"), __func__);

    str->len = len;
    str->data[str->len] = '\0';
}

void nm_str_free(nm_str_t *str)
{
    if (!str)
        return;

    if (str->data != NULL)
        free(str->data);

    str->data = NULL;
    str->alloc_bytes = 0;
    str->len = 0;
}

int nm_str_cmp_st(const nm_str_t *str, const char *text)
{
    if (!str || !str->data || !text)
        return NM_ERR;

    if (strcmp(str->data, text) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_str_cmp_tt(const char *text1, const char *text2)
{
    if (!text1 || !text2)
        return NM_ERR;

    if (strcmp(text1, text2) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_str_case_cmp_tt(const char *text1, const char *text2)
{
    if (!text1 || !text2)
        return NM_ERR;

    if (strcasecmp(text1, text2) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_str_cmp_ss(const nm_str_t *str1, const nm_str_t *str2)
{
    if (!str1 || !str2 || !str1->data || !str2->data)
        return NM_ERR;

    if (strcmp(str1->data, str2->data) != 0)
        return NM_ERR;

    return NM_OK;
}

uint32_t nm_str_stoui(const nm_str_t *str, int base)
{
    uint64_t res;
    char *endp;
    const char *data = nm_str_get(str);

    res = strtoul(data, &endp, base);

    if ((res == ULONG_MAX && errno == ERANGE) || (res > UINT32_MAX))
        nm_bug(_("%s: integer overflow"), __func__);

    if ((endp == data) || (*endp != '\0'))
        nm_bug(_("%s: bad integer value=\'%s\'"), __func__, data);

    return res;
}

static uint64_t nm_str_stoul__(const char *str, int base)
{
    uint64_t res;
    char *endp;

    res = strtoull(str, &endp, base);

    if (res == ULLONG_MAX && errno == ERANGE)
        nm_bug(_("%s: integer overflow"), __func__);

    if ((endp == str) || (*endp != '\0'))
        nm_bug(_("%s: bad integer value=\'%s\'"), __func__, str);

    return res;
}

uint64_t nm_str_stoul(const nm_str_t *str, int base)
{
    return nm_str_stoul__(nm_str_get(str), base);
}

uint64_t nm_str_ttoul(const char *str, int base)
{
    return nm_str_stoul__(str, base);
}

int64_t nm_str_stol(const nm_str_t *str, int base)
{
    int64_t res;
    char *endp;
    const char *data = nm_str_get(str);

    res = strtoll(data, &endp, base);

    if ((res == LLONG_MAX || res == LLONG_MIN) && errno == ERANGE)
        nm_bug(_("%s: integer overflow"), __func__);

    if ((endp == data) || (*endp != '\0'))
        nm_bug(_("%s: bad integer value=\'%s\'"), __func__, data);

    return res;
}

void nm_str_dirname(const nm_str_t *str, nm_str_t *res)
{
    if (!str)
        return;

    const char *data = nm_str_get(str);
    size_t pos;

    pos = str->len;

    if (str->len == 0)
        nm_bug(_("%s: zero length string"), __func__);

    for (size_t n = (pos - 1); n > 0; n--) {
        if (data[n] == '/') {
            pos = n;
            break;
        }
    }

    if ((pos == str->len) && (*data == '/'))
        pos = 1;

    nm_str_alloc_mem(res, data, pos);
}

#if 0
void nm_str_basename(const nm_str_t *str, nm_str_t *res)
{
    if (!str)
        return;

    nm_str_t path = NM_INIT_STR;
    char *path_end;

    nm_str_copy(&path, str);

    if (str->len == 0)
        nm_bug(_("%s: zero length string"), __func__);

    path_end = strrchr(path.data, '/');
    if (path_end == NULL) {
        nm_str_copy(res, str);
        return;
    }

    if (path_end[1] == '\0') {
        while ((path_end > path.data) && (path_end[-1] == '/'))
            --path_end;

        if (path_end > path.data) {
            *path_end-- = '\0';
            while ((path_end > path.data) && (path_end[-1]) != '/')
                --path_end;
        } else {
            while (path_end[1] != '\0')
                ++path_end;
        }
    } else {
        ++path_end;
    }

    nm_str_alloc_mem(res, path_end, strlen(path_end));
    nm_str_free(&path);
}
#endif

void nm_str_append_format(nm_str_t *str, const char *fmt, ...)
{
    int len = 0;
    char *buf = NULL;
    va_list args;

    /* get required length */
    va_start(args, fmt);
    len = vsnprintf(buf, 0, fmt, args);
    va_end(args);

    if (len < 0)
        nm_bug(_("%s: invalid length: %d"), __func__, len);

    len++; /* for \0 */

    buf = nm_calloc(1, len);

    va_start(args, fmt);
    len = vsnprintf(buf, len, fmt, args);
    if (len < 0)
        nm_bug(_("%s: invalid length: %d"), __func__, len);
    va_end(args);

    nm_str_append_mem(str, buf, len);
    free(buf);
}

void nm_str_format(nm_str_t *str, const char *fmt, ...)
{
    int len = 0;
    char *buf = NULL;
    va_list args;

    /* get required length */
    va_start(args, fmt);
    len = vsnprintf(buf, 0, fmt, args);
    va_end(args);

    if (len < 0)
        nm_bug(_("%s: invalid length: %d"), __func__, len);

    len++; /* for \0 */

    buf = nm_calloc(1, len);

    va_start(args, fmt);
    len = vsnprintf(buf, len, fmt, args);
    if (len < 0)
        nm_bug(_("%s: invalid length: %d"), __func__, len);
    va_end(args);

    nm_str_alloc_mem(str, buf, len);
    free(buf);
}

void nm_str_remove_char(nm_str_t *str, char ch)
{
    if (!str)
        return;

    char *prd, *pwr;

    prd = pwr = str->data;

    while (*prd) {
        *pwr = *prd++;
        pwr += (*pwr == ch) ? 0 : 1;
    }

    *pwr = '\0';
}

size_t nm_strlcpy(char *dst, const char *src, size_t buflen)
{
    size_t srclen = strlen(src);
    size_t ret = srclen;

    if (buflen > 0) {
        if (srclen >= buflen)
            srclen = buflen - 1;

        memcpy(dst, src, srclen);
        dst[srclen] = '\0';
    }

    return ret;
}

void nm_str_append_to_vect(const nm_str_t *src, nm_vect_t *v, const char *delim)
{
    char *token, *saveptr;
    nm_str_t buf = NM_INIT_STR;

    nm_str_copy(&buf, src);
    saveptr = buf.data;

    while ((token = strtok_r(saveptr, delim, &saveptr))) {
        nm_vect_insert_cstr(v, token);
    }

    nm_str_free(&buf);
}

void nm_str_vect_ins_cb(void *unit_p, const void *ctx)
{
    nm_str_copy((nm_str_t *) unit_p, (const nm_str_t *) ctx);
}

void nm_str_vect_free_cb(void *unit_p)
{
    nm_str_free((nm_str_t *) unit_p);
}

void nm_str_replace_text(nm_str_t *str, const char *old, const char *new)
{
    nm_str_t buf = NM_INIT_STR;
    size_t count = 0;
    char *ins = NULL;
    char *prev = NULL;
    char *cur = NULL;
    size_t old_len = strlen(old);
    size_t new_len = strlen(new);

    ins = str->data;
    for (count = 0; (cur = strstr(ins, old)); ++count)
        ins = cur + old_len;

    if (count == 0)
        return;

    nm_str_alloc_mem(&buf, NULL, str->len + (new_len * count) - (old_len * count));
    buf.len = 0;

    ins = buf.data;
    prev = str->data;
    while ((cur = strstr(prev, old)))
    {
        memcpy(ins, prev, cur - prev);
        ins += cur - prev;
        buf.len += cur - prev;

        memcpy(ins, new, new_len);
        ins += new_len;
        buf.len += new_len;

        prev += cur - prev + old_len;
    }

    memcpy(ins, prev, str->len - (prev - str->data));
    buf.len += str->len - (prev - str->data);

    if (buf.len != buf.alloc_bytes - 1)
        nm_bug(_("%s: string replace failed"), __func__);

    nm_str_free(str);
    str->alloc_bytes = buf.alloc_bytes;
    str->data = buf.data;
    str->len = buf.len;
}

static void nm_str_alloc_mem(nm_str_t *str, const char *src, size_t len)
{
    if (!str)
        return;

    size_t len_needed;

    if (len + 1 < len)
        nm_bug(_("Integer overflow\n"));

    len_needed = len + 1;

    if (len_needed > str->alloc_bytes) {
        nm_str_free(str);
        str->data = nm_alloc(len_needed);
        str->alloc_bytes = len_needed;
    }

    if (src) {
        memcpy(str->data, src, len);
        str->data[len] = '\0';
    }

    str->len = len;
}

static void nm_str_append_mem(nm_str_t *str, const char *src, size_t len)
{
    if (!str)
        return;

    size_t len_needed;

    if (len + str->len < len)
        nm_bug(_("Integer overflow\n"));

    len_needed = len + str->len;
    if (len_needed + 1 < len_needed)
        nm_bug(_("Integer overflow\n"));

    len_needed++;

    if (len_needed > str->alloc_bytes) {
        str->data = nm_realloc(str->data, len_needed);
        str->alloc_bytes = len_needed;
    }

    memcpy(str->data + str->len, src, len);
    str->data[str->len + len] = '\0';
    str->len += len;
}

static void nm_str_append_mem_opt(nm_str_t *str, const char *src, size_t len)
{
    if (!str)
        return;

    size_t len_needed;

    if (len + str->len < len)
        nm_bug(_("Integer overflow"));

    len_needed = len + str->len;
    if ((len_needed + 1) * 10 < len_needed)
        nm_bug(_("Integer overflow"));

    len_needed++;

    if (len_needed > str->alloc_bytes) {
        str->data = nm_realloc(str->data, len_needed * 10);
        str->alloc_bytes = len_needed * 10;
    }

    memcpy(str->data + str->len, src, len);
    str->data[str->len + len] = '\0';
    str->len += len;
}

static const char *nm_str_get(const nm_str_t *str)
{
    if (!str)
        return NULL;

    if (str->data == NULL) {
        if ((str->alloc_bytes != 0) || (str->len != 0))
            nm_bug(_("Malformed nm_str_t data"));

        nm_str_alloc_mem((nm_str_t *)str, NULL, 0);
    }

    return str->data;
}

/* vim:set ts=4 sw=4: */
