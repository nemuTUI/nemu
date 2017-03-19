#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>

typedef struct nm_ini_node_t nm_ini_node_t;

typedef struct {
    nm_str_t name;
    nm_str_t value;
} nm_ini_value_t;

struct nm_ini_node_t {
    nm_str_t name;
    nm_ini_node_t *next;
    nm_ini_value_t *values;
};

static void nm_ini_list_push(nm_ini_node_t **head, nm_str_t *name);

void *nm_ini_parser_init(const nm_str_t *path)
{
    char *buf;
    int look_for_sec_end = 0;
    nm_str_t sec_name = NM_INIT_STR;
    nm_file_map_t file = NM_INIT_FILE;
    nm_ini_node_t *head = NULL;

    file.name = path;

    nm_map_file(&file);
    buf = file.mp;

    for (off_t n = 0; n < file.size; n++)
    {
        if (look_for_sec_end)
        {
            if (*buf == ']')
            {
                nm_ini_list_push(&head, &sec_name);
                nm_str_free(&sec_name);
                look_for_sec_end = 0;
                buf++;
                continue;
            }
            nm_str_add_char_opt(&sec_name, *buf);
        }
        
        if (*buf == '[')
            look_for_sec_end = 1;

        buf++;
    }

    nm_unmap_file(&file);

    if (look_for_sec_end)
        nm_bug(_("Bad INI file: missing \"]\" at section name"));

    return head;
}

void nm_ini_parser_dump(void *head)
{
    nm_ini_node_t *curr = head;

    if (curr == NULL)
        return;

    while (curr != NULL)
    {
        printf("section: %s\n", curr->name.data);
        curr = curr->next;
    }
}

void nm_ini_parser_free(void *head)
{
    nm_ini_node_t *curr = head;
    nm_ini_node_t *old;

    if (curr == NULL)
        return;

    while (curr != NULL)
    {
        nm_str_free(&curr->name);
        old = curr;
        curr = curr->next;
        free(old);
    }
}

static void nm_ini_list_push(nm_ini_node_t **head, nm_str_t *name)
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
}

/* vim:set ts=4 sw=4 fdm=marker: */
