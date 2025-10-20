#ifndef SP_UTILS_H_
#define SP_UTILS_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "sp_proto.h"

typedef struct {
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;
    float x_ratio;
    float y_ratio;
} scale_info_t;

typedef struct {
    int x;
    int y;
} point_t;

void pr_debug(const spice_t *spice, const char *fmt, ...)
    __attribute__ ((format(printf, 2, 3)));
uint64_t get_ts(const spice_t *spice);
char *b64_encode(const uint8_t *src, size_t len);
uint8_t *encrypt_password(const spice_t *spice,
        const unsigned char *key, size_t *encrypted_len);
int read_spice_packet(const spice_t *spice, int sd, uint8_t *buf, ssize_t len);
void init_scale_info(scale_info_t *info, int src_w, int src_h,
        int dst_w, int dst_h);
point_t convert_mouse_coords(const scale_info_t *info, int x, int y);
void write_bmp_header(FILE *fp, unsigned int width, unsigned int height);
void write_bmp_payload(const spice_t *spice, FILE *fp, spice_bitmap_t *bmp,
        spice_msg_display_draw_copy_t *draw,
        unsigned int width, unsigned int height);
bool spice_connect(spice_t *spice, int channel);
bool spice_disconnect(spice_t *spice, int channel);

#endif /* SP_UTILS_H_ */
/* vim:set ts=4 sw=4: */
