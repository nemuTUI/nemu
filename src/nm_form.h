#ifndef NM_FORM_H_
#define NM_FORM_H_

#include <nm_ncurses.h>

#include <form.h>

typedef FORM nm_form_t;
typedef FIELD nm_field_t;

nm_form_t *nm_post_form(nm_window_t *w, nm_field_t **field, int begin_x);
void nm_draw_form(nm_window_t *w, nm_form_t *form);
void nm_form_free(nm_form_t *form, nm_field_t **fields);

#endif /* NM_FORM_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
