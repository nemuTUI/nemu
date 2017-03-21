#ifndef NM_INI_PARSER_H_
#define NM_INI_PARSER_H_

typedef struct nm_ini_node_s nm_ini_node_t;

void *nm_ini_parser_init(const nm_str_t *path);
void nm_ini_parser_dump(const nm_ini_node_t *head);
void nm_ini_parser_free(nm_ini_node_t *head);
int nm_ini_parser_find(const nm_ini_node_t *head, const char *section,
                       const char *param, nm_str_t *res);

#endif /* NM_INI_PARSER_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
