#include <string.h>
#include <stdlib.h>

#include <libspice.h>

#include "sp_core.h"
#include "sp_utils.h"
#include "sp_proto.h"

#include <spice/protocol.h>

#include <X11/Xutil.h>

#define ATKB_RELEASE   0x80
#define ATKB_X11_SHIFT 0x08
#define ATKB_UP        0x48
#define ATKB_LEFT      0x4b
#define ATKB_RIGHT     0x4d
#define ATKB_DOWN      0x50

static void display_copy_image(const spice_t *spice, const uint8_t *src,
        uint8_t **srcp, spice_image_t **img);
static void display_copy_palette(const spice_t *spice, const uint8_t *src,
        uint8_t **srcp, spice_bitmap_t *dst);
static void display_copy_bitmap(const spice_t *spice, const uint8_t *src,
        const spice_image_t *img, spice_bitmap_t *dst);

static bool spice_init_channel(const spice_t *spice, uint8_t channel_type,
        uint32_t common_caps, uint32_t channel_caps)
{
    uint32_t magic = SPICE_MAGIC;
    unsigned char msg[sizeof(spice_link_header_t) +
        sizeof(spice_link_mess_t) + 2 * sizeof(uint32_t)] = {0};
    unsigned char reply[sizeof(spice_link_header_t) +
        sizeof(spice_link_reply_t)] = {0};
    unsigned char *pmsg = msg;
    spice_link_header_t spice_link_hdr;
    spice_link_mess_t spice_link_mess;
    spice_link_reply_t spice_link_reply;
    size_t cl_caps_len = 0;
    ssize_t srv_caps_len;
    unsigned char *srv_caps_buf;
    const unsigned char *key;
    size_t encrypted_len;
    unsigned char *encrypted = NULL;
    uint32_t result = 0;
    uint32_t session_id = 0;
    int sd;

    switch (channel_type) {
    case SPICE_CHANNEL_MAIN:
        sd = spice->sd_main;
        break;
    case SPICE_CHANNEL_DISPLAY:
        sd = spice->sd_display;
        break;
    case SPICE_CHANNEL_INPUTS:
        sd = spice->sd_inputs;
        break;
    default:
        pr_debug(spice, "unexpected channel type\n");
        return false;
    }

    if (channel_type != SPICE_CHANNEL_MAIN) {
        session_id = spice->session_id;
    }

    if (common_caps) {
        cl_caps_len = sizeof(uint32_t);
    }

    if (channel_caps) {
        cl_caps_len = sizeof(uint32_t) * 2;
    }

    memset(&spice_link_hdr, 0, sizeof(spice_link_hdr));
    memset(&spice_link_mess, 0, sizeof(spice_link_mess));
    memset(&spice_link_reply, 0, sizeof(spice_link_reply));

    spice_link_hdr.magic = magic;
    spice_link_hdr.major_version = SPICE_VERSION_MAJOR;
    spice_link_hdr.minor_version = SPICE_VERSION_MINOR;
    spice_link_hdr.size = sizeof(spice_link_mess) + cl_caps_len;

    memcpy(pmsg, &spice_link_hdr, sizeof(spice_link_hdr));
    pmsg += sizeof(spice_link_hdr);

    spice_link_mess.channel_type = channel_type;
    spice_link_mess.connection_id = session_id;
    spice_link_mess.caps_offset = sizeof(spice_link_mess);
    if (common_caps) {
        spice_link_mess.num_common_caps = 1;
    }
    if (channel_caps) {
        spice_link_mess.num_channel_caps = 1;
    }
    memcpy(pmsg, &spice_link_mess, sizeof(spice_link_mess));
    if (common_caps) {
        memcpy(pmsg + sizeof(spice_link_mess),
                &common_caps, sizeof(common_caps));
    }
    if (channel_caps) {
        memcpy(pmsg + sizeof(spice_link_mess) + sizeof(common_caps),
                &channel_caps, sizeof(channel_caps));
    }

    send(sd, msg, sizeof(spice_link_hdr) +
            sizeof(spice_link_mess) + cl_caps_len, 0);
    recv(sd, &reply, sizeof(reply), 0);
    memcpy(&spice_link_hdr, reply, sizeof(spice_link_hdr));
    memcpy(&spice_link_reply, reply + sizeof(spice_link_hdr),
            sizeof(spice_link_reply));

    if (spice_link_reply.error != SPICE_LINK_ERR_OK) {
        pr_debug(spice, "bad reply: %d\n", spice_link_reply.error);
        return false;
    }

    srv_caps_len = spice_link_hdr.size - spice_link_reply.caps_offset;
    srv_caps_buf = malloc(srv_caps_len);
    read_spice_packet(spice, sd, srv_caps_buf, srv_caps_len);
    free(srv_caps_buf);

    key = spice_link_reply.pub_key;
    encrypted = encrypt_password(spice, key, &encrypted_len);
    if (!encrypted) {
        return false;
    }
    send(sd, encrypted, encrypted_len, 0);
    free(encrypted);

    recv(sd, &result, sizeof(result), 0);
    if (result != SPICE_LINK_ERR_OK) {
        pr_debug(spice, "Ret code: %u\n", result);
        return false;
    }

    return true;
}

SP_EXPORT bool spice_channel_init_main(spice_t *spice)
{
    uint32_t common_caps = 0;
    uint32_t channel_caps = 0;
    spice_mini_data_header_t data_hdr = {0};
    spice_msg_main_init_t main_init_msg = {0};

    if (!spice_connect(spice, SPICE_CHANNEL_MAIN)) {
        return false;
    }

    common_caps |= (1 << SPICE_COMMON_CAP_MINI_HEADER);

    if (spice_init_channel(spice, SPICE_CHANNEL_MAIN,
                common_caps, channel_caps) == false) {
        pr_debug(spice, "%s: failed to create main channel\n", __func__);
        return false;
    }

    read_spice_packet(spice, spice->sd_main,
            (uint8_t *) &data_hdr, sizeof(data_hdr));
    pr_debug(spice, "type: %u, size: %u\n", data_hdr.type, data_hdr.size);

    if (data_hdr.type != SPICE_MSG_MAIN_INIT) {
        pr_debug(spice, "spice, unexpected type: %u\n", data_hdr.type);
        return false;
    }

    read_spice_packet(spice, spice->sd_main,
            (uint8_t *) &main_init_msg, sizeof(main_init_msg));
    pr_debug(spice, "session_id: %u\n", main_init_msg.session_id);
    spice->session_id = main_init_msg.session_id;

    if (main_init_msg.current_mouse_mode != SPICE_MOUSE_MODE_CLIENT) {
        spice_msgc_main_mouse_mode_request_t mouse_msg = {0};
        unsigned char msg_buf[sizeof(data_hdr) + sizeof(mouse_msg)] = {0};

        pr_debug(spice, "set mouse mode to client type\n");

        data_hdr.type = SPICE_MSGC_MAIN_MOUSE_MODE_REQUEST;
        data_hdr.size = sizeof(mouse_msg);
        mouse_msg.mouse_mode = SPICE_MOUSE_MODE_CLIENT;
        memcpy(msg_buf, &data_hdr, sizeof(data_hdr));
        memcpy(msg_buf + sizeof(data_hdr), &mouse_msg, sizeof(mouse_msg));
        send(spice->sd_main, msg_buf, sizeof(msg_buf), 0);
    }

    return true;
}

SP_EXPORT bool spice_channel_init_inputs(spice_t *spice)
{
    if (!spice_connect(spice, SPICE_CHANNEL_INPUTS)) {
        return false;
    }

    if (!spice_init_channel(spice, SPICE_CHANNEL_INPUTS, 0, 0)) {
        pr_debug(spice, "failed to create input channel\n");
        return false;
    }

    return true;
}

SP_EXPORT void *spice_channel_main_loop(void *ctx)
{
    spice_t *spice = ctx;
    struct timeval tv;
    fd_set readset;
    int fd_num;
    uint8_t *buf, *bufp;
    uint32_t data_size;
    int state;
    ssize_t recv_len, recv_len_total;
    spice_mini_data_header_t data_hdr = {0};
    uint8_t *data_hdrp = (uint8_t *) &data_hdr;
    uint32_t ack_frequency = 0;
    uint32_t msg_received = 0;

    recv_len_total = data_size = 0;
    buf = NULL;

    enum {
        RECV_HDR,
        RECV_OTHER_DATA,
        REPLY_TO_PING,
        REPLY_TO_ACK,
    };

    state = RECV_HDR;
    memset(&data_hdr, 0, sizeof(data_hdr));

    for (;;) {
        FD_ZERO(&readset);
        FD_SET(spice->sd_main, &readset);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_num = select(spice->sd_main + 1, &readset, NULL, NULL, &tv);
        if (fd_num == -1) {
            pr_debug(spice, "select error\n");
            break;
        } else if (fd_num && FD_ISSET(spice->sd_main, &readset)) {
            switch (state) {
            case RECV_HDR:
                pr_debug(spice, "* state (main): RECV_HDR\n");

                recv_len = recv(spice->sd_main, data_hdrp, sizeof(data_hdr), 0);
                if (recv_len == 0) {
                    atomic_store(&spice->stop, true);
                    pr_debug(spice, "main channel closed from server side\n");
                    break;
                }
                recv_len_total = recv_len;
                data_hdrp += recv_len;

                if (recv_len_total == sizeof(data_hdr)) {
                    pr_debug(spice, "* state (main): RECV_HDR done. "
                            "Payload size will be: %u\n",
                            data_hdr.size);
                    if (data_hdr.size) {
                        buf = malloc(data_hdr.size);
                        bufp = buf;
                        data_size = data_hdr.size;
                    }

                    switch (data_hdr.type) {
                    case SPICE_MSG_PING:
                        state = REPLY_TO_PING;
                        break;
                    case SPICE_MSG_SET_ACK:
                        state = REPLY_TO_ACK;
                        break;
                    default:
                        pr_debug(spice, "Other (main): %u\n", data_hdr.type);
                        if (data_hdr.size) {
                            state = RECV_OTHER_DATA;
                        }
                        break;
                    }

                    recv_len_total = 0;
                    memset(&data_hdr, 0, sizeof(data_hdr));
                    data_hdrp = (uint8_t *) &data_hdr;
                }
                break;
            case RECV_OTHER_DATA:
                pr_debug(spice, "* state (main): RECV_OTHER_DATA\n");
                recv_len = recv(spice->sd_main, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state (main): RECV_OTHER_DATA. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    msg_received++;
                }
                break;

            case REPLY_TO_PING:
                pr_debug(spice, "* state (main): REPLY_TO_PING\n");
                recv_len = recv(spice->sd_main, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state (main): REPLY_TO_PING. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    spice_msg_ping_t *ping = (spice_msg_ping_t *) buf;
                    spice_msg_ping_t pong = {
                        .id = ping->id,
                        .time = ping->time,
                    };
                    char pong_buf[sizeof(spice_mini_data_header_t) +
                        sizeof(spice_msg_ping_t)] = {0};
                    data_hdr.type = SPICE_MSGC_PONG;
                    data_hdr.size = sizeof(spice_msg_ping_t);
                    memcpy(pong_buf, &data_hdr, sizeof(data_hdr));
                    memcpy(pong_buf + sizeof(spice_mini_data_header_t),
                            &pong, sizeof(pong));
                    send(spice->sd_main, pong_buf, sizeof(pong_buf), 0);
                    pr_debug(spice, "PONG SENDED (main)-> id: %u, ts: %lu\n",
                            pong.id, pong.time);

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    msg_received++;
                }
                break;

            case REPLY_TO_ACK:
                pr_debug(spice, "* state (main): REPLY_TO_ACK\n");
                recv_len = recv(spice->sd_main, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state (main): REPLY_TO_ACK. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    spice_msg_set_ack_t *set_ack = (spice_msg_set_ack_t *) buf;
                    spice_msgc_ack_sync_t ack_sync = {
                        .generation = set_ack->generation
                    };
                    ack_frequency = set_ack->window;
                    char ack_buf[sizeof(spice_mini_data_header_t) +
                        sizeof(spice_msgc_ack_sync_t)] = {0};
                    data_hdr.type = SPICE_MSGC_ACK_SYNC;
                    data_hdr.size = sizeof(spice_msgc_ack_sync_t);
                    memcpy(ack_buf, &data_hdr, sizeof(data_hdr));
                    memcpy(ack_buf + sizeof(spice_mini_data_header_t),
                            &ack_sync, sizeof(ack_sync));
                    send(spice->sd_main, ack_buf, sizeof(ack_buf), 0);
                    pr_debug(spice, "ACK SYNC SENDED (main)-> gen: %u\n",
                            ack_sync.generation);

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                }
                break;
            }

        } else {
            pr_debug(spice, "timeout...(main)\n");
        }

        if (ack_frequency && ack_frequency == msg_received) {
            data_hdr.type = SPICE_MSGC_ACK;
            data_hdr.size = 0;
            send(spice->sd_main, &data_hdr, sizeof(data_hdr), 0);
            pr_debug(spice, "SPICE_MSGC_ACK sended (main)\n");
            msg_received = 0;
        }

        if (atomic_load(&spice->stop)) {
            pr_debug(spice, "stopping main channel\n");
            break;
        }
    }

    return NULL;
}

SP_EXPORT void *spice_channel_display_loop(void *ctx)
{
    uint32_t common_caps = 0;
    uint32_t channel_caps = 0;
    spice_t *spice = ctx;
    struct timeval tv;
    fd_set readset;
    int fd_num;
    uint8_t *buf, *bufp;
    uint32_t width, height, data_size;
    int state;
    ssize_t recv_len, recv_len_total;
    spice_mini_data_header_t data_hdr = {0};
    uint8_t *data_hdrp = (uint8_t *) &data_hdr;
    spice_msgc_display_init_t display_init = {0};
    spice_msgc_preferred_compression_t comp = {0};
    char cache_buf[sizeof(spice_mini_data_header_t) +
        sizeof(spice_msgc_display_init_t)] = {0};
    char comp_buf[sizeof(spice_mini_data_header_t) +
        sizeof(spice_msgc_preferred_compression_t)] = {0};
    uint32_t ack_frequency;
    uint32_t msg_received;
    FILE *fp;

channel_init:
    ack_frequency = msg_received = recv_len_total = width =
        height = data_size = 0;
    buf = NULL;

    if (!spice_connect(spice, SPICE_CHANNEL_DISPLAY)) {
        return NULL;
    }

    common_caps |= (1 << SPICE_COMMON_CAP_MINI_HEADER);
    channel_caps |= (1 << SPICE_DISPLAY_CAP_PREF_COMPRESSION);

    if (spice_init_channel(spice, SPICE_CHANNEL_DISPLAY,
                common_caps, channel_caps) == false) {
        pr_debug(spice, "%s: failed to create display channel\n", __func__);
        return NULL;
    }

    data_hdr.type = SPICE_MSGC_DISPLAY_INIT;
    data_hdr.size = sizeof(spice_msgc_display_init_t);
    memcpy(cache_buf, &data_hdr, sizeof(data_hdr));
    memcpy(cache_buf + sizeof(spice_mini_data_header_t),
            &display_init, sizeof(display_init));
    send(spice->sd_display, cache_buf, sizeof(cache_buf), 0);

    data_hdr.type = SPICE_MSGC_DISPLAY_PREFERRED_COMPRESSION;
    data_hdr.size = sizeof(spice_msgc_preferred_compression_t);
    comp.image_compression = SPICE_IMAGE_COMPRESSION_OFF;
    memcpy(comp_buf, &data_hdr, sizeof(data_hdr));
    memcpy(comp_buf + sizeof(spice_mini_data_header_t),
            &comp, sizeof(comp));
    send(spice->sd_display, comp_buf, sizeof(comp_buf), 0);

    enum {
        RECV_HDR,
        RECV_SURFACE_DATA,
        RECV_SURFACE_DESTROY,
        RECV_COPY_DATA,
        RECV_OTHER_DATA,
        REPLY_TO_PING,
        REPLY_TO_ACK,
    };

    state = RECV_HDR;
    memset(&data_hdr, 0, sizeof(data_hdr));

    for (;;) {
        FD_ZERO(&readset);
        FD_SET(spice->sd_display, &readset);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_num = select(spice->sd_display + 1, &readset, NULL, NULL, &tv);
        if (fd_num == -1) {
            pr_debug(spice, "select error\n");
            break;
        } else if (fd_num && FD_ISSET(spice->sd_display, &readset)) {
            switch (state) {
            case RECV_HDR:
                pr_debug(spice, "* state: RECV_HDR\n");

                recv_len = recv(spice->sd_display, data_hdrp,
                        sizeof(data_hdr), 0);
                recv_len_total = recv_len;
                data_hdrp += recv_len;

                if (recv_len_total == sizeof(data_hdr)) {
                    pr_debug(spice, "* state: RECV_HDR done. "
                            "Payload size will be: %u\n", data_hdr.size);
                    if (data_hdr.size) {
                        buf = malloc(data_hdr.size);
                        bufp = buf;
                        data_size = data_hdr.size;
                    }

                    switch (data_hdr.type) {
                    case SPICE_MSG_DISPLAY_SURFACE_CREATE:
                        state = RECV_SURFACE_DATA;
                        break;
                    case SPICE_MSG_DISPLAY_SURFACE_DESTROY:
                        state = RECV_SURFACE_DESTROY;
                        break;
                    case SPICE_MSG_DISPLAY_DRAW_COPY:
                        state = RECV_COPY_DATA;
                        break;
                    case SPICE_MSG_PING:
                        state = REPLY_TO_PING;
                        break;
                    case SPICE_MSG_SET_ACK:
                        state = REPLY_TO_ACK;
                        break;
                    default:
                        pr_debug(spice, "Other: %u\n", data_hdr.type);
                        if (data_hdr.size) {
                            state = RECV_OTHER_DATA;
                        }
                        break;
                    }

                    recv_len_total = 0;
                    memset(&data_hdr, 0, sizeof(data_hdr));
                    data_hdrp = (uint8_t *) &data_hdr;
                }
                break;
            case RECV_SURFACE_DATA:
                pr_debug(spice, "* state: RECV_SURFACE_DATA\n");
                recv_len = recv(spice->sd_display, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state: RECV_SURFACE_DATA. "
                            "Need: %u, got %zu\n", data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    spice_msg_surface_create_t *surface_create;
                    size_t sizeloc = 0;

                    surface_create = (spice_msg_surface_create_t *) buf;
                    pr_debug(spice, "surface_id: %u, height: %u, "
                            "width: %u, format: %u\n",
                            surface_create->surface_id,
                            surface_create->height, surface_create->width,
                            surface_create->format);
                    width = surface_create->width;
                    height = surface_create->height;

                    fp = open_memstream(&spice->bmp_buf, &sizeloc);
                    write_bmp_header(fp, width, height);

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    msg_received++;
                }
                break;
            case RECV_SURFACE_DESTROY:
                pr_debug(spice, "* state: RECV_SURFACE_DESTROY\n");
                recv_len = recv(spice->sd_display, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state: RECV_SURFACE_DESTROY. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    spice_msg_surface_destroy_t *surface_destroy;

                    surface_destroy = (spice_msg_surface_destroy_t *) buf;
                    pr_debug(spice, "destroy surface_id: %u\n",
                            surface_destroy->surface_id);
                    atomic_store(&spice->data_ready, false);
                    fclose(fp);
                    free(spice->bmp_buf);
                    spice->bmp_buf = NULL;
                    fp = NULL;

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    spice_disconnect(spice, SPICE_CHANNEL_DISPLAY);
                    goto channel_init;
                }
                break;
            case RECV_COPY_DATA:
                pr_debug(spice, "* state: RECV_COPY_DATA\n");
                recv_len = recv(spice->sd_display, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state: RECV_COPY_DATA. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    int mask_copy_len;
                    spice_bitmap_t bmp = {0};
                    spice_msg_display_draw_copy_t draw = {0};
                    uint8_t *src = buf;

                    memcpy(&draw.base.surface_id, src,
                            sizeof(draw.base.surface_id));
                    src += sizeof(draw.base.surface_id);
                    memcpy(&draw.base.box, src, sizeof(draw.base.box));
                    src += sizeof(draw.base.box);
                    memcpy(&draw.base.clip.type, src,
                            sizeof(draw.base.clip.type));
                    src += sizeof(draw.base.clip.type);
                    if (draw.base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                        pr_debug(spice,
                                "WARN: clip type is SPICE_CLIP_TYPE_RECTS, "
                                "not supported:\n  use `-vga virtio` "
                                "instead of `-vga qxl` as a workaround\n");
                        draw.base.clip.rects = (spice_clip_rects_t *) src;
                        src += sizeof(draw.base.clip.rects->num_rects);
                        src += draw.base.clip.rects->num_rects *
                            sizeof(spice_rect_t);
                    } else {
                        pr_debug(spice,
                                "clip type is not SPICE_CLIP_TYPE_RECTS: %u\n",
                                draw.base.clip.type);
                    }

                    display_copy_image(spice, buf, &src, &draw.data.src_bitmap);

                    if (!draw.data.src_bitmap) {
                        pr_debug(spice, "image is not bitmaps!\n");
                    } else {
                        pr_debug(spice, "image OK: %ux%u\n",
                                draw.data.src_bitmap->descriptor.width,
                                draw.data.src_bitmap->descriptor.height);
                    }

                    memcpy(&draw.data.pixels, src, sizeof(draw.data.pixels));
                    src += sizeof(draw.data.pixels);

                    mask_copy_len = sizeof(draw.data.mask.flags) +
                        sizeof(draw.data.mask.pos);
                    memcpy(&draw.data.mask, src, mask_copy_len);
                    src += mask_copy_len;

                    display_copy_image(spice, buf,
                            &src, &draw.data.mask.bitmap);

                    pr_debug(spice, "img type: %u\n",
                            draw.data.src_bitmap->descriptor.type);
                    display_copy_bitmap(spice, buf, draw.data.src_bitmap, &bmp);
                    write_bmp_payload(spice, fp, &bmp, &draw, width, height);
                    atomic_store(&spice->data_ready, true);

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    msg_received++;
                }
                break;
            case RECV_OTHER_DATA:
                pr_debug(spice, "* state: RECV_OTHER_DATA\n");
                recv_len = recv(spice->sd_display, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state: RECV_OTHER_DATA. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    msg_received++;
                }
                break;

            case REPLY_TO_PING:
                pr_debug(spice, "* state: REPLY_TO_PING\n");
                recv_len = recv(spice->sd_display, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state: REPLY_TO_PING. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    spice_msg_ping_t *ping = (spice_msg_ping_t *) buf;
                    spice_msg_ping_t pong = {
                        .id = ping->id,
                        .time = ping->time,
                    };
                    char pong_buf[sizeof(spice_mini_data_header_t) +
                        sizeof(spice_msg_ping_t)] = {0};
                    data_hdr.type = SPICE_MSGC_PONG;
                    data_hdr.size = sizeof(spice_msg_ping_t);
                    memcpy(pong_buf, &data_hdr, sizeof(data_hdr));
                    memcpy(pong_buf + sizeof(spice_mini_data_header_t),
                            &pong, sizeof(pong));
                    send(spice->sd_display, pong_buf, sizeof(pong_buf), 0);
                    pr_debug(spice, "PONG SENDED-> id: %u, ts: %lu\n",
                            pong.id, pong.time);

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                    msg_received++;
                }
                break;

            case REPLY_TO_ACK:
                pr_debug(spice, "* state: REPLY_TO_ACK\n");
                recv_len = recv(spice->sd_display, bufp,
                        data_size - recv_len_total, 0);
                if (recv_len > 0) {
                    recv_len_total += recv_len;
                    bufp += recv_len;
                    pr_debug(spice, "* state: REPLY_TO_ACK. "
                            "Need: %u, got %zu\n",
                            data_size, recv_len_total);
                }

                if (recv_len_total == data_size) {
                    spice_msg_set_ack_t *set_ack = (spice_msg_set_ack_t *) buf;
                    spice_msgc_ack_sync_t ack_sync = {
                        .generation = set_ack->generation
                    };
                    ack_frequency = set_ack->window;
                    pr_debug(spice, "channel display ack window: %u\n",
                            ack_frequency);
                    char ack_buf[sizeof(spice_mini_data_header_t) +
                        sizeof(spice_msgc_ack_sync_t)] = {0};
                    data_hdr.type = SPICE_MSGC_ACK_SYNC;
                    data_hdr.size = sizeof(spice_msgc_ack_sync_t);
                    memcpy(ack_buf, &data_hdr, sizeof(data_hdr));
                    memcpy(ack_buf + sizeof(spice_mini_data_header_t),
                            &ack_sync, sizeof(ack_sync));
                    send(spice->sd_display, ack_buf, sizeof(ack_buf), 0);
                    pr_debug(spice, "ACK SYNC SENDED-> gen: %u\n",
                            ack_sync.generation);

                    state = RECV_HDR;
                    recv_len_total = 0;
                    free(buf);
                    buf = NULL;
                }
                break;
            }

        } else {
            pr_debug(spice, "timeout...\n");
        }

        if (ack_frequency && ack_frequency == msg_received) {
            data_hdr.type = SPICE_MSGC_ACK;
            data_hdr.size = 0;
            send(spice->sd_display, &data_hdr, sizeof(data_hdr), 0);
            pr_debug(spice, "SPICE_MSGC_ACK sended\n");
            msg_received = 0;
        }
        if (atomic_load(&spice->stop)) {
            free(buf);
            fclose(fp);
            spice_disconnect(spice, SPICE_CHANNEL_DISPLAY);
            pr_debug(spice, "stopping display channel\n");
            break;
        }
    }

    system("clear");

    return NULL;
}

static void display_copy_image(const spice_t *spice, const uint8_t *src,
        uint8_t **srcp, spice_image_t **img)
{
    uint32_t offset = 0;

    memcpy(&offset, *srcp, sizeof(offset));
    pr_debug(spice, "%s: offset: %d\n", __func__, offset);
    *srcp += sizeof(offset);
    *img = (spice_image_t *) (offset > 0 ? src + offset : NULL);
}

static void display_copy_palette(const spice_t *spice, const uint8_t *src,
        uint8_t **srcp, spice_bitmap_t *dst)
{
    uint32_t offset = 0;

    memcpy(&offset, *srcp, sizeof(offset));
    pr_debug(spice, "%s: offset: %d\n", __func__, offset);
    *srcp += sizeof(offset);

    if (offset) {
        dst->palette = (spice_palette_t *) src + offset;
        memcpy(&dst->palette_id, *srcp, sizeof(dst->palette_id));
        *srcp += sizeof(dst->palette_id);

        return;
    }

    dst->palette = NULL;
    dst->palette_id = 0;
}

static void display_copy_bitmap(const spice_t *spice, const uint8_t *src,
        const spice_image_t *img, spice_bitmap_t *dst)
{
    uint8_t *p = (uint8_t *) &img->bitmap;

    int len =
        sizeof(dst->format) +
        sizeof(dst->flags) +
        sizeof(dst->x) +
        sizeof(dst->y) +
        sizeof(dst->stride);

    memcpy(dst, p, len);
    p += len;

    display_copy_palette(spice, src, &p, dst);
    dst->data = p;
}

SP_EXPORT void spice_send_key(spice_t *spice, uint32_t keycode,
        KeySym keysym, bool up)
{
    int sd = spice->sd_inputs;
    spice_data_header_t input_data_hdr = {0};
    spice_msgc_key_t key = {0};
    unsigned char buf[sizeof(input_data_hdr) + sizeof(key)] = {0};

    keycode -= ATKB_X11_SHIFT;

    input_data_hdr.serial = spice->serial++;
    input_data_hdr.type = up ?
        SPICE_MSGC_INPUTS_KEY_UP : SPICE_MSGC_INPUTS_KEY_DOWN;
    input_data_hdr.size = sizeof(key);

    switch (keysym) {
    case XK_Up:
        keycode = ATKB_UP;
        break;
    case XK_Left:
        keycode = ATKB_LEFT;
        break;
    case XK_Right:
        keycode = ATKB_RIGHT;
        break;
    case XK_Down:
        keycode = ATKB_DOWN;
        break;
    }

    if (up) {
        key.code[0] = keycode | ATKB_RELEASE;
    } else {
        key.code[0] = keycode;
    }

    memcpy(buf, &input_data_hdr, sizeof(input_data_hdr));
    memcpy(buf + sizeof(input_data_hdr), &key, sizeof(key));
    send(sd, buf, sizeof(buf), 0);
}

SP_EXPORT void spice_send_mouse_motion(spice_t *spice, int x, int y)
{
    spice_msgc_mouse_position_t pos = {0};
    spice_data_header_t input_data_hdr = {0};
    unsigned char buf[sizeof(input_data_hdr) + sizeof(pos)] = {0};
    scale_info_t scale;
    point_t point;

    init_scale_info(&scale,
            spice->term_width_px, spice->term_height_px,
            spice->disp_width_px, spice->disp_height_px);

    point = convert_mouse_coords(&scale, x, y);

    pos.x = point.x;
    pos.y = point.y;

    input_data_hdr.serial = spice->serial++;
    input_data_hdr.type = SPICE_MSGC_INPUTS_MOUSE_POSITION;
    input_data_hdr.size = sizeof(pos);
    memcpy(buf, &input_data_hdr, sizeof(input_data_hdr));
    memcpy(buf + sizeof(input_data_hdr), &pos, sizeof(pos));
    send(spice->sd_inputs, buf, sizeof(buf), 0);
}

void spice_send_mouse_button(spice_t *spice, unsigned int button, bool up)
{
    spice_msgc_mouse_button_t mouse = {0};
    spice_data_header_t input_data_hdr = {0};
    unsigned char buf[sizeof(input_data_hdr) + sizeof(mouse)] = {0};

    mouse.button = button;
    switch (mouse.button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        if (up) {
            spice->button_state &= ~SPICE_MOUSE_BUTTON_MASK_LEFT;
        } else {
            spice->button_state |= SPICE_MOUSE_BUTTON_MASK_LEFT;
        }
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        if (up) {
            spice->button_state &= ~SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        } else {
            spice->button_state |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        }
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        if (up) {
            spice->button_state &= ~SPICE_MOUSE_BUTTON_MASK_RIGHT;
        } else {
            spice->button_state |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
        }
        break;
    }

    mouse.button_state = spice->button_state;
    input_data_hdr.serial = spice->serial++;
    input_data_hdr.type = up ?
        SPICE_MSGC_INPUTS_MOUSE_RELEASE : SPICE_MSGC_INPUTS_MOUSE_PRESS;
    input_data_hdr.size = sizeof(mouse);
    memcpy(buf, &input_data_hdr, sizeof(input_data_hdr));
    memcpy(buf + sizeof(input_data_hdr), &mouse, sizeof(mouse));
    send(spice->sd_inputs, buf, sizeof(buf), 0);
}
/* vim:set ts=4 sw=4: */
