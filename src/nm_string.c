#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>

static void nm_str_alloc_mem(nm_str_t *str, const char *src, size_t len);
static void nm_str_append_mem(nm_str_t *str, const char *src, size_t len);
static void nm_str_append_mem_opt(nm_str_t *str, const char *src, size_t len);
static const char *nm_str_get(const nm_str_t *str);

void nm_str_alloc_text(nm_str_t *str, const char *src)
{
    size_t len = strlen(src);
    nm_str_alloc_mem(str, src, len);
}

void nm_str_alloc_str(nm_str_t *str, const nm_str_t *src)
{
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

void nm_str_add_str(nm_str_t *str, const nm_str_t *src)
{
    nm_str_append_mem(str, src->data, src->len);
}

void nm_str_copy(nm_str_t *str, const nm_str_t *src)
{
    nm_str_alloc_mem(str, src->data, src->len);
}

void nm_str_trunc(nm_str_t *str, size_t len)
{
    if (len >= str->alloc_bytes)
        nm_bug(_("%s: bad length"), __func__);

    str->len = len;
    str->data[str->len] = '\0';
}

void nm_str_free(nm_str_t *str)
{
    if (str->data != 0)
        free(str->data);

    str->data = NULL;
    str->alloc_bytes = 0;
    str->len = 0;
}

int nm_str_cmp_st(const nm_str_t *str, const char *text)
{
    if (strncmp(str->data, text, str->len) != 0)
        return NM_ERR;

    return NM_OK;
}

int nm_str_cmp_ss(const nm_str_t *str1, const nm_str_t *str2)
{
    if (strncmp(str1->data, str2->data, str1->len) != 0)
        return NM_ERR;

    return NM_OK;
}

uint32_t nm_str_stoui(const nm_str_t *str)
{
    uint64_t res;
    char *endp;
    const char *data = nm_str_get(str);

    res = strtoul(data, &endp, 10);

    if ((errno == ERANGE) || (res > UINT32_MAX))
        nm_bug(_("%s: integer overflow"), __func__);

    if ((endp == data) || (*endp != '\0'))
        nm_bug(_("%s: bad integer value=%s"), __func__, data);

    return res;
}

uint64_t nm_str_stoul(const nm_str_t *str)
{
    uint64_t res;
    char *endp;
    const char *data = nm_str_get(str);

    res = strtoull(data, &endp, 10);

    if (errno == ERANGE)
        nm_bug(_("%s: integer overflow"), __func__);

    if ((endp == data) || (*endp != '\0'))
        nm_bug(_("%s: bad integer value=%s"), __func__, data);

    return res;
}

void nm_str_dirname(const nm_str_t *str, nm_str_t *res)
{
    const char *data = nm_str_get(str);
    size_t pos = str->len;

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

    nm_str_alloc_mem(res, data, pos);
}

static void nm_str_alloc_mem(nm_str_t *str, const char *src, size_t len)
{
    size_t len_needed;

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
    if (str->data == NULL)
    {
        if ((str->alloc_bytes != 0) || (str->len != 0))
            nm_bug(_("Malformed nm_str_t data"));

        nm_str_alloc_mem((nm_str_t *)str, NULL, 0);
    }

    return str->data;
}

/* vim:set ts=4 sw=4 fdm=marker: */
