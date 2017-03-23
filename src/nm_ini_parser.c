#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_ini_parser.h>

typedef struct nm_ini_value_s nm_ini_value_t;

struct nm_ini_value_s {
    nm_str_t name;
    nm_str_t value;
    nm_ini_value_t *next;
};

struct nm_ini_node_s {
    nm_str_t name;
    nm_ini_node_t *next;
    nm_ini_value_t *values_head;
};

static nm_ini_node_t *nm_ini_node_push(nm_ini_node_t **head, nm_str_t *name);
static void nm_ini_value_push(nm_ini_node_t *head, nm_str_t *name, nm_str_t *value);

nm_ini_node_t *nm_ini_parser_init(const nm_str_t *path)
{
    char *buf_ini;
    int look_for_sec_end = 0;
    int look_for_param_end = 0;
    int look_for_value_end = 0;
    int comment_block = 0;
    nm_str_t sec_name = NM_INIT_STR;
    nm_str_t par_name = NM_INIT_STR;
    nm_str_t par_value = NM_INIT_STR;
    nm_file_map_t file = NM_INIT_FILE;
    nm_ini_node_t *head = NULL;
    nm_ini_node_t *curr_node;

    file.name = path;

    nm_map_file(&file);
    buf_ini = file.mp;

    for (off_t n = 0; n < file.size; n++, buf_ini++)
    {
        /* skip spaces and tabs */
        if ((*buf_ini == 0x20) || (*buf_ini == '\t'))
            continue;

        if (*buf_ini == '[')
        {
            look_for_sec_end = 1;
            continue;
        }

        /* {{{ skip comments '#...' */
        if (comment_block && (*buf_ini != '\n'))
            continue;

        if (*buf_ini == 0x23)
        {
            comment_block = 1;
            continue;
        }
        if (comment_block && (*buf_ini == '\n'))
        {
            comment_block = 0;
            continue;
        } /* }}} comments */

        /* {{{ collect section names */
        if (look_for_sec_end)
        {
            if (*buf_ini == ']')
            {
                curr_node = nm_ini_node_push(&head, &sec_name);
                nm_str_free(&sec_name);
                look_for_sec_end = 0;
                look_for_param_end = 1;
                continue;
            }
            nm_str_add_char_opt(&sec_name, *buf_ini);
        } /* }}} section names */

        /* {{{ collect param names */
        if (look_for_param_end && !look_for_sec_end)
        {
            if (*buf_ini == '=')
            {
                look_for_param_end = 0;
                look_for_value_end = 1;
                continue;
            }
            if (*buf_ini != '\n')
                nm_str_add_char_opt(&par_name, *buf_ini);
        } /* }}} param */

        /* {{{ collect values */
        if (look_for_value_end)
        {
            if (*buf_ini == '\n')
            {
                look_for_value_end = 0;
                look_for_param_end = 1;
                nm_ini_value_push(curr_node, &par_name, &par_value);
                nm_str_free(&par_name);
                nm_str_free(&par_value);
                continue;
            }
            nm_str_add_char_opt(&par_value, *buf_ini);
        } /* }}} */
    }

    nm_unmap_file(&file);

    if (look_for_sec_end)
        nm_bug(_("Bad INI file: missing \"]\" at section name"));

    return head;
}

#if (NM_DEBUG)
void nm_ini_parser_dump(const nm_ini_node_t *head)
{
    const nm_ini_node_t *curr = head;

    if (curr == NULL)
        return;

    nm_debug("-=ini cfg start=-\n");
    while (curr != NULL)
    {
        const nm_ini_value_t *values = curr->values_head;

        nm_debug("section: %s\n", curr->name.data);
        while (values != NULL)
        {
            nm_debug("\t%s = %s\n", values->name.data, values->value.data);
            values = values->next;
        }

        curr = curr->next;
    }
    nm_debug("-=ini cfg end=-\n");
}
#endif

void nm_ini_parser_free(nm_ini_node_t *head)
{
    nm_ini_node_t *curr = head;

    if (curr == NULL)
        return;

    while (curr != NULL)
    {
        nm_ini_node_t *old = curr;
        nm_ini_value_t *values = curr->values_head;

        while (values != NULL)
        {
            nm_ini_value_t *old_val = values;
            nm_str_free(&values->name);
            nm_str_free(&values->value);
            values = values->next;
            free(old_val);
        }

        nm_str_free(&curr->name);
        curr = curr->next;
        free(old);
    }
}

static nm_ini_node_t *nm_ini_node_push(nm_ini_node_t **head, nm_str_t *name)
{
    nm_ini_node_t *curr = *head;

    if (curr == NULL) /* list is empty */
    {
        curr = nm_calloc(1, sizeof(nm_ini_node_t));
        *head = curr;
        goto fill_data;
    }

    while (curr->next != NULL)
        curr = curr->next;

    curr->next = nm_calloc(1, sizeof(nm_ini_node_t));
    curr = curr->next;

fill_data:
    nm_str_copy(&curr->name, name);

    return curr;
}

static void nm_ini_value_push(nm_ini_node_t *head, nm_str_t *name, nm_str_t *value)
{
    nm_ini_value_t *curr;

    if (head == NULL)
        nm_bug(_("NULL list head pointer"));

    curr = head->values_head;

    if (curr == NULL) /* list is empty */
    {
        curr = nm_calloc(1, sizeof(nm_ini_value_t));
        head->values_head = curr;
        goto fill_data;
    }

    while (curr->next != NULL)
        curr = curr->next;

    curr->next = nm_calloc(1, sizeof(nm_ini_value_t));
    curr = curr->next;

fill_data:
    nm_str_copy(&curr->name, name);
    nm_str_copy(&curr->value, value);
}

int nm_ini_parser_find(const nm_ini_node_t *head, const char *section,
                       const char *param, nm_str_t *res)
{
    const nm_ini_node_t *curr = head;
    int ret = NM_ERR, found = 0;

    if (curr == NULL)
        nm_bug(_("NULL list head pointer"));

    while (curr != NULL)
    {
        const nm_ini_value_t *values;

        if (found)
            break;

        if (nm_str_cmp_st(&curr->name, section) == NM_OK)
        {
            values = curr->values_head;
            found = 1;

            while (values != NULL)
            {
                if (nm_str_cmp_st(&values->name, param) == NM_OK)
                {
                    ret = NM_OK;
                    nm_str_copy(res, &values->value);
                    break;
                }

                values = values->next;
            }
        }

        curr = curr->next;
    }

    return ret;
}

/* vim:set ts=4 sw=4 fdm=marker: */
