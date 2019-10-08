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

void nm_str_alloc_str(nm_str_t *str, const nm_str_t *src)
{
    assert(src != NULL);
    nm_str_alloc_mem(str, src->data, src->len);
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
    assert(src != NULL);
    nm_str_append_mem(str, src, len);
}

void nm_str_add_str(nm_str_t *str, const nm_str_t *src)
{
    assert(src != NULL);
    nm_str_append_mem(str, src->data, src->len);
}

void nm_str_add_str_part(nm_str_t *str, const nm_str_t *src, size_t len)
{
    assert(src != NULL);
    nm_str_append_mem(str, src->data, len);
}

void nm_str_copy(nm_str_t *str, const nm_str_t *src)
{
    assert(src != NULL);
    nm_str_alloc_mem(str, src->data, src->len);
}

void nm_str_trunc(nm_str_t *str, size_t len)
{
    assert(str != NULL);

    if (len >= str->alloc_bytes)
        nm_bug(_("%s: bad length"), __func__);

    str->len = len;
    str->data[str->len] = '\0';
}

void nm_str_free(nm_str_t *str)
{
    assert(str != NULL);

    if (str->data != NULL)
        free(str->data);

    str->data = NULL;
    str->alloc_bytes = 0;
    str->len = 0;
}

int nm_str_cmp_st(const nm_str_t *str, const char *text)
{
    assert(str != NULL);

    if (strcmp(str->data, text) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_str_cmp_tt(const char *text1, const char *text2)
{
    if (strcmp(text1, text2) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_str_cmp_ss(const nm_str_t *str1, const nm_str_t *str2)
{
    assert(str1 != NULL);
    assert(str2 != NULL);

    if (strcmp(str1->data, str2->data) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_strn_cmp_ss(const nm_str_t *str1, const nm_str_t *str2)
{
    assert(str1 != NULL);
    assert(str2 != NULL);

    if (strncmp(str1->data, str2->data, strlen(str1->data)) != 0)
        return NM_ERR;

    return NM_OK;
}

uint32_t nm_str_stoui(const nm_str_t *str, int base)
{
    uint64_t res;
    char *endp;
    const char *data = nm_str_get(str);

    res = strtoul(data, &endp, base);

    if ((errno == ERANGE) || (res > UINT32_MAX))
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

    if (errno == ERANGE)
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

    if (errno == ERANGE)
        nm_bug(_("%s: integer overflow"), __func__);

    if ((endp == data) || (*endp != '\0'))
        nm_bug(_("%s: bad integer value=\'%s\'"), __func__, data);

    return res;
}

void nm_str_dirname(const nm_str_t *str, nm_str_t *res)
{
    const char *data = nm_str_get(str);
    size_t pos;

    assert(str != NULL);
    pos = str->len;

    if (str->len == 0)
        nm_bug(_("%s: zero length string"), __func__);

    for (size_t n = (pos - 1); n > 0; n--)
    {
        if (data[n] == '/')
        {
            pos = n;
            break;
        }
    }

    if ((pos == str->len) && (*data == '/'))
        pos = 1;

    nm_str_alloc_mem(res, data, pos);
}

void nm_str_basename(const nm_str_t *str, nm_str_t *res)
{
    nm_str_t path = NM_INIT_STR;
    char *path_end;

    assert(str != NULL);
    nm_str_copy(&path, str);

    if (str->len == 0)
        nm_bug(_("%s: zero length string"), __func__);

    path_end = strrchr(path.data, '/');
    if (path_end == NULL)
    {
        nm_str_copy(res, str);
        return;
    }

    if (path_end[1] == '\0')
    {
        while ((path_end > path.data) && (path_end[-1] == '/'))
            --path_end;

        if (path_end > path.data)
        {
            *path_end-- = '\0';
            while ((path_end > path.data) && (path_end[-1]) != '/')
                --path_end;
        }
        else
        {
            while (path_end[1] != '\0')
                ++path_end;
        }
    }
    else
    {
        ++path_end;
    }

    nm_str_alloc_mem(res, path_end, strlen(path_end));
    nm_str_free(&path);
}

void nm_str_append_format(nm_str_t *str, const char *fmt, ...)
{
    int len = 0;
    char *buf = NULL;
    va_list args;

    /* get required length */
    va_start(args, fmt);
    len = vsnprintf(buf, len, fmt, args);
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
    len = vsnprintf(buf, len, fmt, args);
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
    char *prd, *pwr;

    assert(str != NULL);
    prd = pwr = str->data;

    while (*prd)
    {
        *pwr = *prd++;
        pwr += (*pwr == ch) ? 0 : 1;
    }

    *pwr = '\0';
}

size_t nm_strlcpy(char *dst, const char *src, size_t buflen)
{
    size_t srclen = strlen(src);
    size_t ret = srclen;

    if (buflen > 0)
    {
        if (srclen >= buflen)
            srclen = buflen - 1;

        memcpy(dst, src, srclen);
        dst[srclen] = '\0';
    }

    return ret;
}

void nm_str_vect_ins_cb(void *unit_p, const void *ctx)
{
    nm_str_copy((nm_str_t *) unit_p, (const nm_str_t *) ctx);
}

void nm_str_vect_free_cb(void *unit_p)
{
    nm_str_free((nm_str_t *) unit_p);
}

static void nm_str_alloc_mem(nm_str_t *str, const char *src, size_t len)
{
    size_t len_needed;

    assert(str != NULL);

    if (len + 1 < len)
        nm_bug(_("Integer overflow\n"));

    len_needed = len + 1;

    if (len_needed > str->alloc_bytes)
    {
        nm_str_free(str);
        str->data = nm_alloc(len_needed);
        str->alloc_bytes = len_needed;
    }

    memcpy(str->data, src, len);
    str->data[len] = '\0';
    str->len = len;
}

static void nm_str_append_mem(nm_str_t *str, const char *src, size_t len)
{
    size_t len_needed;

    assert(str != NULL);

    if (len + str->len < len)
        nm_bug(_("Integer overflow\n"));

    len_needed = len + str->len;
    if (len_needed + 1 < len_needed)
        nm_bug(_("Integer overflow\n"));

    len_needed++;

    if (len_needed > str->alloc_bytes)
    {
        str->data = nm_realloc(str->data, len_needed);
        str->alloc_bytes = len_needed;
    }

    memcpy(str->data + str->len, src, len);
    str->data[str->len + len] = '\0';
    str->len += len;
}

static void nm_str_append_mem_opt(nm_str_t *str, const char *src, size_t len)
{
    size_t len_needed;

    assert(str != NULL);

    if (len + str->len < len)
        nm_bug(_("Integer overflow"));

    len_needed = len + str->len;
    if ((len_needed + 1) * 10 < len_needed)
        nm_bug(_("Integer overflow"));

    len_needed++;

    if (len_needed > str->alloc_bytes)
    {
        str->data = nm_realloc(str->data, len_needed * 10);
        str->alloc_bytes = len_needed * 10;
    }

    memcpy(str->data + str->len, src, len);
    str->data[str->len + len] = '\0';
    str->len += len;
}

static const char *nm_str_get(const nm_str_t *str)
{
    assert(str != NULL);

    if (str->data == NULL)
    {
        if ((str->alloc_bytes != 0) || (str->len != 0))
            nm_bug(_("Malformed nm_str_t data"));

        nm_str_alloc_mem((nm_str_t *)str, NULL, 0);
    }

    return str->data;
}

/* vim:set ts=4 sw=4: */
