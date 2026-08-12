#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "detect_plugin.h"
#include "star6e.h"

typedef MI_S32 (*mma_alloc_fn)(const char *, MI_U32, MI_U64 *);
typedef MI_S32 (*mma_free_fn)(MI_U64);
typedef MI_S32 (*mmap_fn)(MI_U64, MI_U32, void **, MI_BOOL);
typedef MI_S32 (*munmap_fn)(void *, MI_U32);

static unsigned char clamp8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

/* RGB -> resized NV12 640x352, nearest-neighbour.
 * Fast enough because Mission Mode runs only every >=10 seconds.
 */
static void rgb_to_nv12(const unsigned char *rgb,
                        int sw, int sh,
                        unsigned char *dst,
                        int dw, int dh)
{
    unsigned char *yplane = dst;
    unsigned char *uvplane = dst + dw * dh;

    for (int y = 0; y < dh; y++) {
        int sy = (int)((int64_t)y * sh / dh);

        for (int x = 0; x < dw; x++) {
            int sx = (int)((int64_t)x * sw / dw);
            const unsigned char *p = rgb + (sy * sw + sx) * 3;

            int r = p[0];
            int g = p[1];
            int b = p[2];

            int Y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yplane[y * dw + x] = clamp8(Y);
        }
    }

    for (int y = 0; y < dh; y += 2) {
        for (int x = 0; x < dw; x += 2) {
            int sum_u = 0;
            int sum_v = 0;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 2; xx++) {
                    int dx = x + xx;
                    int dy = y + yy;
                    int sx = (int)((int64_t)dx * sw / dw);
                    int sy = (int)((int64_t)dy * sh / dh);

                    const unsigned char *p = rgb + (sy * sw + sx) * 3;

                    int r = p[0];
                    int g = p[1];
                    int b = p[2];

                    int U = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    int V = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

                    sum_u += clamp8(U);
                    sum_v += clamp8(V);
                }
            }

            int uv = (y / 2) * dw + x;
            uvplane[uv]     = (unsigned char)(sum_u / 4);
            uvplane[uv + 1] = (unsigned char)(sum_v / 4);
        }
    }
}


static void put_green(unsigned char *img, int w, int h, int x, int y)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return;

    unsigned char *p = img + (y * w + x) * 3;
    p[0] = 0;
    p[1] = 255;
    p[2] = 0;
}

static void draw_rect(unsigned char *img, int w, int h,
                      int x1, int y1, int x2, int y2)
{
    int t;

    for (t = 0; t < 3; t++) {
        for (int x = x1; x <= x2; x++) {
            put_green(img, w, h, x, y1 + t);
            put_green(img, w, h, x, y2 - t);
        }

        for (int y = y1; y <= y2; y++) {
            put_green(img, w, h, x1 + t, y);
            put_green(img, w, h, x2 - t, y);
        }
    }
}

/* 5x7: digits 0-9 plus '%' */
static const unsigned char font5x7[11][7] = {
    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},
    {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {17,2,4,8,17,0,0}
};

static void draw_char(unsigned char *img, int w, int h,
                      int x, int y, char c, int scale)
{
    int idx;

    if (c >= '0' && c <= '9')
        idx = c - '0';
    else if (c == '%')
        idx = 10;
    else
        return;

    for (int yy = 0; yy < 7; yy++) {
        for (int xx = 0; xx < 5; xx++) {
            if (font5x7[idx][yy] & (1 << (4 - xx))) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        put_green(img, w, h,
                                  x + xx * scale + sx,
                                  y + yy * scale + sy);
            }
        }
    }
}

static void draw_label(unsigned char *img, int w, int h,
                       int x, int y, int cls, float score)
{
    char text[32];

    /* kujul: 3 64% */
    snprintf(text, sizeof(text), "%d %d%%",
             cls, (int)(score * 100.0f + 0.5f));

    int px = x;

    for (const char *c = text; *c; c++) {
        if (*c == ' ') {
            px += 8;
            continue;
        }

        draw_char(img, w, h, px, y, *c, 2);
        px += 12;
    }
}

int main(int argc, char **argv)
{
    const int W = 640;
    const int H = 352;
    const MI_U32 nv12_size = W * H * 3 / 2;

    void *sys_h = NULL;
    void *plugin_h = NULL;

    mma_alloc_fn mma_alloc = NULL;
    mma_free_fn mma_free = NULL;
    mmap_fn sys_mmap = NULL;
    munmap_fn sys_munmap = NULL;

    MI_U64 phy = 0;
    void *vir = NULL;

    unsigned char *rgb = NULL;
    int iw = 0, ih = 0, channels = 0;

    WaybeamDetectEntryFn entry;
    const DetectBackend *backend;

    DetectBackendConfig cfg;
    DetectFrame frame;
    DetectBox boxes[32];

    int count = 0;
    int ret;
    int exit_code = 2;

    if (argc != 2) {
        fprintf(stderr, "usage: %s IMAGE.jpg\n", argv[0]);
        return 2;
    }

    rgb = stbi_load(argv[1], &iw, &ih, &channels, 3);
    if (!rgb) {
        fprintf(stderr, "JPEG decode failed: %s\n", argv[1]);
        return 2;
    }

    sys_h = dlopen("libmi_sys.so", RTLD_NOW | RTLD_GLOBAL);
    if (!sys_h) {
        fprintf(stderr, "libmi_sys: %s\n", dlerror());
        goto out;
    }

    mma_alloc = (mma_alloc_fn)dlsym(sys_h, "MI_SYS_MMA_Alloc");
    mma_free  = (mma_free_fn)dlsym(sys_h, "MI_SYS_MMA_Free");
    sys_mmap  = (mmap_fn)dlsym(sys_h, "MI_SYS_Mmap");
    sys_munmap = (munmap_fn)dlsym(sys_h, "MI_SYS_Munmap");

    if (!mma_alloc || !mma_free || !sys_mmap || !sys_munmap) {
        fprintf(stderr, "MI_SYS symbols missing\n");
        goto out;
    }

    ret = mma_alloc(NULL, nv12_size, &phy);
    if (ret != 0 || !phy)
        ret = mma_alloc("mma_heap_name0", nv12_size, &phy);

    if (ret != 0 || !phy) {
        fprintf(stderr, "MMA alloc failed\n");
        goto out;
    }

    ret = sys_mmap(phy, nv12_size, &vir, 0);
    if (ret != 0 || !vir) {
        fprintf(stderr, "MMA mmap failed\n");
        goto out;
    }

    rgb_to_nv12(rgb, iw, ih, vir, W, H);

    plugin_h = dlopen("/tmp/libmission_detect.so",
                      RTLD_NOW | RTLD_GLOBAL);
    if (!plugin_h) {
        fprintf(stderr, "plugin: %s\n", dlerror());
        goto out;
    }

    entry = (WaybeamDetectEntryFn)dlsym(plugin_h,
                                        WAYBEAM_DETECT_ENTRY);
    if (!entry) {
        fprintf(stderr, "detector entry missing\n");
        goto out;
    }

    backend = entry();
    if (!backend) {
        fprintf(stderr, "detector backend missing\n");
        goto out;
    }

    memset(&cfg, 0, sizeof(cfg));

    cfg.model_path = "/tmp/yolov8n352drone.img";
    cfg.firmware_path = "/config/dla/ipu_firmware.bin";

    cfg.net_width = W;
    cfg.net_height = H;

    cfg.display_w = W;
    cfg.display_h = H;

    cfg.conf_thresh = 0.25f;
    cfg.nms_iou = 0.45f;

    if (backend->init(&cfg) != 0) {
        fprintf(stderr, "detector init failed\n");
        goto out;
    }

    memset(&frame, 0, sizeof(frame));

    frame.width = W;
    frame.height = H;
    frame.stride_y = W;
    frame.stride_uv = W;
    frame.phy_y = phy;
    frame.phy_uv = phy + (MI_U64)W * H;

    ret = backend->process(&frame, boxes, 32, &count);

    if (ret != 0) {
        fprintf(stderr, "detector process failed: %d\n", ret);
        backend->deinit();
        goto out;
    }

    backend->deinit();

    if (count > 0) {

        for (int n = 0; n < count; n++) {

            printf("BOX %d cls=%d score=%.3f net=(%.1f,%.1f)-(%.1f,%.1f)\\n",
                   n, boxes[n].cls, boxes[n].score,
                   boxes[n].x1, boxes[n].y1,
                   boxes[n].x2, boxes[n].y2);

            int x1 = (int)(boxes[n].x1 * iw / 640.0f);
            int y1 = (int)(boxes[n].y1 * ih / 352.0f);
            int x2 = (int)(boxes[n].x2 * iw / 640.0f);
            int y2 = (int)(boxes[n].y2 * ih / 352.0f);

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= iw) x2 = iw - 1;
            if (y2 >= ih) y2 = ih - 1;

            draw_rect(rgb, iw, ih, x1, y1, x2, y2);

            int ly = y1 >= 20 ? y1 - 18 : y1 + 5;
            draw_label(rgb, iw, ih, x1 + 3, ly,
                       boxes[n].cls, boxes[n].score);
        }

        if (!stbi_write_jpg(argv[1], iw, ih, 3, rgb, 92)) {
            fprintf(stderr, "annotated JPEG write failed\n");
            exit_code = 2;
        } else {
            printf("DETECTED count=%d annotated\n", count);
            exit_code = 1;
        }

    } else {
        printf("CLEAR\n");
        exit_code = 0;
    }

out:
    if (plugin_h)
        dlclose(plugin_h);

    if (vir && sys_munmap)
        sys_munmap(vir, nv12_size);

    if (phy && mma_free)
        mma_free(phy);

    if (sys_h)
        dlclose(sys_h);

    if (rgb)
        stbi_image_free(rgb);

    return exit_code;
}
