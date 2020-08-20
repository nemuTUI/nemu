#ifndef NM_SVG_MAP_H_
#define NM_SVG_MAP_H_

#include <nm_vector.h>

enum {
    NM_SVG_STATE_UP,
    NM_SVG_STATE_DOWN,
    NM_SVG_STATE_ALL
};

void nm_svg_map(const char *path, const nm_vect_t *veths,
        int state, const nm_str_t *layout, const nm_str_t *group);

#endif /* NM_SVG_MAP_H_ */
/* vim:set ts=4 sw=4: */
