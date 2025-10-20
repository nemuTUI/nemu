#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <libspice.h>

#include "sp_core.h"
#include "sp_utils.h"
#include "sp_proto.h"

#include <png.h>

#define PNG_SHM_PATH "/nemu-spice-dump"

typedef struct {
    spice_t *spice;
    uint8_t *data;
    size_t size;
    size_t allocated;
} png_mem_buf_t;

static void png_mem_write_cb(png_structp pngp, png_bytep data, png_size_t len)
{
    png_mem_buf_t *buf = png_get_io_ptr(pngp);

    if (buf->size + len > buf->allocated) {
        pr_debug(buf->spice, "%s: unexpected PNG length\n", __func__);
        exit(EXIT_FAILURE);
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
}

SP_EXPORT void *spice_draw_screen(void *ctx)
{
    spice_t *spice = ctx;
    struct winsize win;
    char *png_path = b64_encode((const uint8_t *) PNG_SHM_PATH,
            strlen(PNG_SHM_PATH));
    uint8_t *bmp;
    int bmp_row_size, png_row_size;
    bmp_header_t bmp_hdr;
    png_structp pngp;
    png_infop infop;
    png_bytep *rowp;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    spice->term_width_px = win.ws_xpixel;
    spice->term_height_px = win.ws_ypixel;

    system("clear");

    for (;;) {
        png_mem_buf_t png_buf = {0};
        int shm_fd;
        size_t max_png_len;

        if (atomic_load(&spice->stop)) {
            pr_debug(spice, "stopping draw screen thread...\n");
            break;
        }

        if (!atomic_load(&spice->data_ready)) {
            usleep(50000);
            continue;
        }

        bmp = (uint8_t *) spice->bmp_buf;
        memcpy(&bmp_hdr, bmp, sizeof(bmp_hdr));
        spice->disp_width_px = bmp_hdr.width_px;
        spice->disp_height_px = bmp_hdr.height_px;
        bmp += bmp_hdr.offset;

        // width * height * channels + PNG headers
        // (a conservative estimate of the maximum PNG file size)
        max_png_len = bmp_hdr.width_px * bmp_hdr.height_px * 3 + 256;
        shm_fd = shm_open(PNG_SHM_PATH, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            pr_debug(spice, "%s: shm_open error\n", __func__);
            exit(EXIT_FAILURE);
        }
        if (ftruncate(shm_fd, max_png_len) == -1) {
            pr_debug(spice, "%s: ftruncate error\n", __func__);
            exit(EXIT_FAILURE);
        }
        png_buf.spice = spice;
        png_buf.allocated = max_png_len;
        png_buf.data = mmap(NULL, max_png_len,
                PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (png_buf.data == MAP_FAILED) {
            pr_debug(spice, "%s: error map BMP\n", __func__);
            exit(EXIT_FAILURE);
        }

        pngp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!pngp) {
            pr_debug(spice, "%s: error create PNG struct\n", __func__);
            exit(EXIT_FAILURE);
        }

        infop = png_create_info_struct(pngp);
        png_set_write_fn(pngp, &png_buf, png_mem_write_cb, NULL);
        png_set_IHDR(pngp, infop,
                bmp_hdr.width_px, bmp_hdr.height_px, 8, PNG_COLOR_TYPE_RGB,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
        png_write_info(pngp, infop);

        rowp = malloc(sizeof(png_bytep) * bmp_hdr.height_px);
        bmp_row_size = bmp_hdr.width_px * 4;
        png_row_size = bmp_hdr.width_px * 3;

        for (int y = 0; y < bmp_hdr.height_px; y++) {
            int bmp_row = bmp_hdr.height_px - 1 - y;
            uint8_t *bmp_row_data = bmp + bmp_row * bmp_row_size;
            png_bytep png_row;

            png_row = (png_bytep) malloc(png_row_size);
            rowp[y] = png_row;

            for (int x = 0; x < bmp_hdr.width_px; x++) {
                uint8_t *bmp_pixel = bmp_row_data + x * 4;
                uint8_t *png_pixel = png_row + x * 3;

                png_pixel[0] = bmp_pixel[2]; // R
                png_pixel[1] = bmp_pixel[1]; // G
                png_pixel[2] = bmp_pixel[0]; // B
            }
        }

        png_write_image(pngp, rowp);
        png_write_end(pngp, NULL);
        for (int y = 0; y < bmp_hdr.height_px; y++) {
            free(rowp[y]);
        }

        png_destroy_write_struct(&pngp, &infop);
        free(rowp);

        munmap(png_buf.data, max_png_len);
        close(shm_fd);
        printf("\x1b_Gi=42,q=2,a=T,C=1,c=%u,r=%u,t=s,f=100;%s\x1b\x5c",
                win.ws_col, win.ws_row, png_path);

        fflush(stdout);
        atomic_store(&spice->data_ready, false);
    }

    free(png_path);

    return NULL;
}
/* vim:set ts=4 sw=4: */
