#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>

static void nm_str_alloc_mem(nm_str_t *str, const char *src, size_t len);
static void nm_str_append_mem(nm_str_t *str, const char *src, size_t len);
static void nm_str_append_mem_opt(nm_str_t *str, const char *src, size_t len);

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

void nm_str_free(nm_str_t *str)
{
    if (str->data != 0)
        free(str->data);

    str->data = NULL;
    str->alloc_bytes = 0;
    str->len = 0;
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
    if (len_needed + 1 < len_needed)
        nm_bug(_("Integer overflow"));

    len_needed++;

    if (len_needed * 10 < len_needed)
        nm_bug(_("Integer overflow"));

    if (len_needed > str->alloc_bytes)
    {
        str->data = nm_realloc(str->data, len_needed * 10);
        str->alloc_bytes = len_needed * 10;
    }

    memcpy(str->data + str->len, src, len);
    str->data[str->len + len] = '\0';
    str->len += len;
}
/* vim:set ts=4 sw=4 fdm=marker: */
