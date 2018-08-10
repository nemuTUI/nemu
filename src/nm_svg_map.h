#ifndef NM_SVG_MAP_H_
#define NM_SVG_MAP_H_

enum {
    NM_SVG_LAYER_UP,
    NM_SVG_LAYER_DOWN,
    NM_SVG_LAYER_ALL
};

void nm_svg_map(const char *path, const nm_vect_t *veths, int layer);

#endif /* NM_SVG_MAP_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
