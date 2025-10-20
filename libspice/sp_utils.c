#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <libspice.h>

#include "sp_core.h"
#include "sp_utils.h"
#include "sp_proto.h"

#include <openssl/pem.h>
#include <openssl/err.h>

#include <spice/protocol.h>

SP_EXPORT void spice_cancel(spice_t *spice)
{
    atomic_store(&spice->stop, true);
}

SP_EXPORT bool spice_is_canceled(const spice_t *spice)
{
    if (atomic_load(&spice->stop)) {
        return true;
    }

    return false;
}

SP_EXPORT spice_t *spice_init(const char *addr, uint16_t port, const char *log)
{
    spice_t *spice;

    spice = calloc(1, sizeof(spice_t));
    if (spice == NULL) {
        return NULL;
    }

    spice->stop = ATOMIC_VAR_INIT(false);
    spice->data_ready = ATOMIC_VAR_INIT(false);

    spice->endpoint.sin_family = AF_INET;
    spice->endpoint.sin_port = htons(port);
    inet_pton(AF_INET, addr, &spice->endpoint.sin_addr);

    spice->serial = 1;

    if (log) {
        spice->log = true;
        spice->logfp = fopen(log, "a+");
    }

    return spice;
}

SP_EXPORT void spice_deinit(spice_t *spice)
{
    spice_disconnect(spice, SPICE_CHANNEL_INPUTS);
    spice_disconnect(spice, SPICE_CHANNEL_MAIN);

    if (spice->log) {
        fclose(spice->logfp);
    }

    free(spice->bmp_buf);
    free(spice);
}

bool spice_connect(spice_t *spice, int channel)
{
    int sd = -1, opt = 1;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef __linux__
    setsockopt(sd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));
#endif

    if (connect(sd, (struct sockaddr *) &spice->endpoint,
                sizeof(struct sockaddr_in)) < 0) {
        pr_debug(spice, "connect: %s\n", strerror(errno));
        return false;
    }

    switch (channel) {
    case SPICE_CHANNEL_MAIN:
        spice->sd_main = sd;
        break;
    case SPICE_CHANNEL_DISPLAY:
        spice->sd_display = sd;
        break;
    case SPICE_CHANNEL_INPUTS:
        spice->sd_inputs = sd;
        break;
    default:
        pr_debug(spice, "unexpected channel type\n");
        return false;
    }

    return true;
}

bool spice_disconnect(spice_t *spice, int channel)
{
    int sd;
    spice_msgc_disconnect_t disconnect  = {0};

    switch (channel) {
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

    disconnect.time_stamp = get_ts(spice);
    disconnect.reason = SPICE_LINK_ERR_OK;

    send(sd, &disconnect, sizeof(disconnect), 0);

    shutdown(sd, SHUT_WR);

    return true;
}

void pr_debug(const spice_t *spice, const char *fmt, ...)
{
    va_list args;

    if (!spice->log) {
        return;
    }

    va_start(args, fmt);
    vfprintf(spice->logfp, fmt, args);
    va_end(args);

    fflush(spice->logfp);
}

uint64_t get_ts(const spice_t *spice)
{
    struct timespec time;
    int rc = clock_gettime(CLOCK_MONOTONIC, &time);

    if (rc != 0) {
        pr_debug(spice, "%s: error get time\n", __func__);
        return 0;
    }

    return (uint64_t) time.tv_sec * 1000LL + time.tv_nsec / 1000000LL;
}

char *b64_encode(const uint8_t *src, size_t len)
{
    char *encoded;
    BIO *bio, *b64;
    BUF_MEM *buf;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, src, len);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buf);

    encoded = malloc(buf->length + 1);
    memcpy(encoded, buf->data, buf->length);
    encoded[buf->length] = '\0';

    BIO_free_all(bio);

    return encoded;
}

uint8_t *encrypt_password(const spice_t *spice,
        const unsigned char *key, size_t *encrypted_len)
{
    unsigned char *encrypted = NULL;
    const char *password = "password";
    size_t password_len = strlen(password);
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = d2i_PUBKEY(NULL, &key, SPICE_TICKET_PUBKEY_BYTES);

    if (!pkey) {
        pr_debug(spice, "Error reading public key: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        pr_debug(spice, "EVP_PKEY_CTX_new: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        pr_debug(spice, "EVP_PKEY_encrypt_init: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }
    EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha1()) <= 0) {
        pr_debug(spice, "EVP_PKEY_encrypt_init: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha1()) <= 0) {
        pr_debug(spice, "EVP_PKEY_CTX_set_rsa_mgf1_md: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, NULL, 0);

    if (EVP_PKEY_encrypt(ctx, NULL, encrypted_len,
                (unsigned char *)password, password_len) <= 0) {
        pr_debug(spice, "EVP_PKEY_encrypt: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    encrypted = malloc(*encrypted_len);
    if (!encrypted) {
        pr_debug(spice, "Memory allocation failed\n");
        goto out;
    }

    if (EVP_PKEY_encrypt(ctx, encrypted, encrypted_len,
                (unsigned char *)password, password_len) <= 0) {
        pr_debug(spice, "EVP_PKEY_encrypt: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
    }

out:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return encrypted;
}

int read_spice_packet(const spice_t *spice, int sd, uint8_t *buf, ssize_t len)
{
    ssize_t remain = len;

    while (remain) {
        ssize_t recv_len = recv(sd, buf, remain, 0);

        if (recv_len == 0) { /* end-of-file */
            pr_debug(spice, "%s: no data...\n", __func__);
            return -1;
        }
        if (recv_len < 0) {
            pr_debug(spice, "%s: error...\n", __func__);
            return -2;
        }
        buf += recv_len;
        remain -= recv_len;
        pr_debug(spice, "%s: got %ld len of %ld\n", __func__, recv_len, len);
    }

    return 0;
}

void init_scale_info(scale_info_t *info, int src_w, int src_h,
        int dst_w, int dst_h)
{
    info->src_width = src_w;
    info->src_height = src_h;
    info->dst_width = dst_w;
    info->dst_height = dst_h;
    info->x_ratio = (float) dst_w / src_w;
    info->y_ratio = (float) dst_h / src_h;
}

point_t convert_mouse_coords(const scale_info_t *info, int x, int y)
{
    point_t point;

    point.x = (int) round(x * info->x_ratio);
    point.y = (int) round(y * info->y_ratio);

    if (point.x < 0) {
        point.x = 0;
    }

    if (point.x >= info->dst_width) {
        point.x = info->dst_width - 1;
    }

    if (point.y < 0) {
        point.y = 0;
    }

    if (point.y >= info->dst_height) {
        point.y = info->dst_height - 1;
    }

    return point;
}

void write_bmp_header(FILE *fp, unsigned int width, unsigned int height)
{
    bmp_header_t hdr = {
        .type = 0x4d42,
        .size = sizeof(bmp_header_t) + height * width * 4,
        .offset = sizeof(bmp_header_t),
        .dib_header_size = 40,
        .width_px = width,
        .height_px = height,
        .num_planes = 1,
        .bits_per_pixel = 32,
        .image_size_bytes = height * width * 4,
        .x_resolution_ppm = 0,
        .y_resolution_ppm = 0,
    };

    fseek(fp, 0, SEEK_SET);
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fflush(fp);
}

void write_bmp_payload(
        const spice_t *spice,
        FILE *fp,
        spice_bitmap_t *bmp,
        spice_msg_display_draw_copy_t *draw,
        unsigned int width,
        unsigned int height)
{
    bool top_down = bmp->flags & SPICE_BITMAP_FLAGS_TOP_DOWN;
    uint8_t *src;

    pr_debug(spice, "BMP: x: %d y: %d w: %d h: %d stride: %d height: %u\n",
            draw->base.box.top, draw->base.box.left,
            width, height, bmp->stride, bmp->y);

    if (top_down) {
        uint8_t *src = bmp->data;

        pr_debug(spice, "%s: top_down %u\n", __func__, bmp->y);

        for (uint32_t n = 0; n < bmp->y; ++n) {
            int dst = (width * 4 * (height - (draw->base.box.top + n))) +
                draw->base.box.left * 4;
            fseek(fp, sizeof(bmp_header_t) + dst, SEEK_SET);
            fwrite(src, bmp->stride, 1, fp);
            src += bmp->stride;
        }
        goto out;
    }

    pr_debug(spice, "%s: !top_down\n", __func__);
    src = bmp->data + bmp->y * bmp->stride;
    for (uint32_t n = 0; n < bmp->y; ++n) {
        int dst = (width * 4 * (height - (draw->base.box.top + n))) +
            draw->base.box.left * 4;
        fseek(fp, sizeof(bmp_header_t) + dst, SEEK_SET);
        src -= bmp->stride;
        fwrite(src, bmp->stride, 1, fp);
    }
out:
    fflush(fp);
}
/* vim:set ts=4 sw=4: */
