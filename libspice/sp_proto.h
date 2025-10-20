#ifndef SP_PROTO_H_
#define SP_PROTO_H_

#include <spice/protocol.h>

#include <libspice.h>

#define SPICE_ATTR_PACKED __attribute__((packed))

typedef SpiceLinkHeader spice_link_header_t;
typedef SpiceLinkMess spice_link_mess_t;
typedef SpiceLinkReply spice_link_reply_t;
typedef SpiceDataHeader spice_data_header_t;
typedef SpiceMiniDataHeader spice_mini_data_header_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgPing {
    uint32_t id;
    uint64_t time;
} spice_msg_ping_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgSetAck {
    uint32_t generation;
    uint32_t window;
} spice_msg_set_ack_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcAckSync {
    uint32_t generation;
} spice_msgc_ack_sync_t;

typedef struct SPICE_ATTR_PACKED SpiceImageDescriptor {
    uint64_t id;
    uint8_t type;
    uint8_t flags;
    uint32_t width;
    uint32_t height;
} spice_image_descriptor_t;

typedef struct SPICE_ATTR_PACKED SpicePalette {
    uint64_t unique;
    uint16_t num_ents;
    uint32_t ents[];
} spice_palette_t;

typedef struct SPICE_ATTR_PACKED SpiceBitmap {
    uint8_t format;
    uint8_t flags;
    uint32_t x;
    uint32_t y;
    uint32_t stride;
    spice_palette_t *palette;
    uint64_t palette_id;
    uint8_t *data;
} spice_bitmap_t;

typedef struct SPICE_ATTR_PACKED SpiceImage {
    spice_image_descriptor_t descriptor;
    spice_bitmap_t bitmap;
} spice_image_t;

typedef struct SPICE_ATTR_PACKED SpiceRect {
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
} spice_rect_t;

typedef struct SPICE_ATTR_PACKED SpiceClipRects {
    uint32_t num_rects;
    spice_rect_t rects[];
} spice_clip_rects_t;

typedef struct SPICE_ATTR_PACKED SpiceClip {
    uint8_t type;
    spice_clip_rects_t *rects;
} spice_clip_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgDisplayBase {
    uint32_t surface_id;
    spice_rect_t box;
    spice_clip_t clip;
} spice_msg_display_base_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgSurfaceCreate {
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
} spice_msg_surface_create_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgSurfaceDestroy {
    uint32_t surface_id;
} spice_msg_surface_destroy_t;

typedef struct SPICE_ATTR_PACKED SpicePoint {
    int32_t x;
    int32_t y;
} spice_point_t;

typedef struct SpiceQMask {
    uint8_t flags;
    spice_point_t pos;
    spice_image_t *bitmap;
} spice_qmask_t;

typedef struct SpiceCopy {
    spice_image_t *src_bitmap;
    struct SPICE_ATTR_PACKED {
        spice_rect_t source_area;
        uint16_t rop_descriptor;
        uint8_t scale_mode;
    } pixels;
    spice_qmask_t mask;
} spice_copy_t;

typedef struct SpiceMsgDisplayDrawCopy {
    spice_msg_display_base_t base;
    spice_copy_t data;
} spice_msg_display_draw_copy_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgMainInit {
    uint32_t session_id;
    uint32_t display_channels_hint;
    uint32_t supported_mouse_modes;
    uint32_t current_mouse_mode;
    uint32_t agent_connected;
    uint32_t agent_tokens;
    uint32_t multi_media_time;
    uint32_t ram_hint;
} spice_msg_main_init_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgDisplayMode {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} spice_msg_display_mode_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcDisplayInit {
    uint8_t cache_id;
    int64_t cache_size;
    uint8_t dictionary_id;
    uint32_t dictionary_window_size;
} spice_msgc_display_init_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcPreferredCompression {
    uint8_t image_compression;
} spice_msgc_preferred_compression_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcDisconnect {
    uint64_t time_stamp;
    uint32_t reason;
} spice_msgc_disconnect_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcKey {
    uint8_t code[4];
} spice_msgc_key_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcMousePosition {
    uint32_t x;
    uint32_t y;
    uint16_t button_state;
    uint8_t display_id;
} spice_msgc_mouse_position_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcMouseButton {
    uint8_t button;
    uint16_t button_state;
} spice_msgc_mouse_button_t;

typedef struct SPICE_ATTR_PACKED SpiceMsgcMainMouseModeRequest {
    uint16_t mouse_mode;
} spice_msgc_main_mouse_mode_request_t;

typedef struct SPICE_ATTR_PACKED {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
    uint32_t dib_header_size;
    int32_t width_px;
    int32_t height_px;
    uint16_t num_planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size_bytes;
    int32_t x_resolution_ppm;
    int32_t y_resolution_ppm;
    uint32_t num_colors;
    uint32_t important_colors;
} bmp_header_t;

#endif /* SP_PROTO_H_ */
/* vim:set ts=4 sw=4: */
