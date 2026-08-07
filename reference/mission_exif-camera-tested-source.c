#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put16le(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void put32le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static void put_rational(unsigned char *p, uint32_t num, uint32_t den) {
    put32le(p, num);
    put32le(p + 4, den);
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len < 2 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    unsigned char *data = malloc((size_t)len);
    if (!data) {
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *size = (size_t)len;
    return data;
}

static void decimal_to_dms(double value, uint32_t out[6]) {
    double a = fabs(value);
    uint32_t deg = (uint32_t)floor(a);
    double min_full = (a - deg) * 60.0;
    uint32_t min = (uint32_t)floor(min_full);
    double sec = (min_full - min) * 60.0;

    out[0] = deg;
    out[1] = 1;
    out[2] = min;
    out[3] = 1;
    out[4] = (uint32_t)llround(sec * 100000.0);
    out[5] = 100000;
}

static int iso_to_exif(const char *iso, char out[20]) {
    int y, m, d, hh, mm, ss;

    if (sscanf(
            iso,
            "%d-%d-%dT%d:%d:%d",
            &y, &m, &d, &hh, &mm, &ss
        ) != 6) {
        return -1;
    }

    snprintf(
        out,
        20,
        "%04d:%02d:%02d %02d:%02d:%02d",
        y, m, d, hh, mm, ss
    );

    return 0;
}

static int write_exif(
    const char *path,
    const char *timestamp,
    double lat,
    double lon,
    double alt,
    double heading
) {
    size_t jpeg_size = 0;
    unsigned char *jpeg = read_file(path, &jpeg_size);

    if (!jpeg) {
        fprintf(stderr, "Cannot read %s\n", path);
        return 1;
    }

    if (jpeg_size < 2 || jpeg[0] != 0xff || jpeg[1] != 0xd8) {
        fprintf(stderr, "Invalid JPEG\n");
        free(jpeg);
        return 1;
    }

    char exif_time[20];
    if (iso_to_exif(timestamp, exif_time) != 0) {
        fprintf(stderr, "Invalid timestamp\n");
        free(jpeg);
        return 1;
    }

    unsigned char exif[1024];
    memset(exif, 0, sizeof(exif));

    memcpy(exif, "Exif\0\0", 6);

    unsigned char *tiff = exif + 6;
    memcpy(tiff, "II", 2);
    put16le(tiff + 2, 42);
    put32le(tiff + 4, 8);

    unsigned char *ifd0 = tiff + 8;
    put16le(ifd0, 2);

    unsigned char *e = ifd0 + 2;

    put16le(e + 0, 0x8769);
    put16le(e + 2, 4);
    put32le(e + 4, 1);
    put32le(e + 8, 38);

    e += 12;

    put16le(e + 0, 0x8825);
    put16le(e + 2, 4);
    put32le(e + 4, 1);
    put32le(e + 8, 76);

    put32le(ifd0 + 2 + 24, 0);

    unsigned char *exif_ifd = tiff + 38;
    put16le(exif_ifd, 1);

    e = exif_ifd + 2;
    put16le(e + 0, 0x9003);
    put16le(e + 2, 2);
    put32le(e + 4, 20);
    put32le(e + 8, 58);
    put32le(exif_ifd + 14, 0);

    memcpy(tiff + 58, exif_time, 19);
    tiff[58 + 19] = '\0';

    unsigned char *gps = tiff + 76;
    put16le(gps, 7);

    unsigned char *g = gps + 2;

    put16le(g + 0, 0x0000);
    put16le(g + 2, 1);
    put32le(g + 4, 4);
    g[8] = 2;
    g[9] = 3;

    g += 12;
    put16le(g + 0, 0x0001);
    put16le(g + 2, 2);
    put32le(g + 4, 2);
    g[8] = lat >= 0 ? 'N' : 'S';

    g += 12;
    put16le(g + 0, 0x0002);
    put16le(g + 2, 5);
    put32le(g + 4, 3);
    put32le(g + 8, 166);

    g += 12;
    put16le(g + 0, 0x0003);
    put16le(g + 2, 2);
    put32le(g + 4, 2);
    g[8] = lon >= 0 ? 'E' : 'W';

    g += 12;
    put16le(g + 0, 0x0004);
    put16le(g + 2, 5);
    put32le(g + 4, 3);
    put32le(g + 8, 190);

    g += 12;
    put16le(g + 0, 0x0005);
    put16le(g + 2, 1);
    put32le(g + 4, 1);
    g[8] = alt < 0 ? 1 : 0;

    g += 12;
    put16le(g + 0, 0x0006);
    put16le(g + 2, 5);
    put32le(g + 4, 1);
    put32le(g + 8, 214);

    g += 12;
    put16le(g + 0, 0x0011);
    put16le(g + 2, 5);
    put32le(g + 4, 1);
    put32le(g + 8, 222);

    put32le(gps + 2 + 7 * 12, 0);

    uint32_t dms[6];

    decimal_to_dms(lat, dms);
    for (int i = 0; i < 3; i++) {
        put_rational(tiff + 166 + i * 8, dms[i * 2], dms[i * 2 + 1]);
    }

    decimal_to_dms(lon, dms);
    for (int i = 0; i < 3; i++) {
        put_rational(tiff + 190 + i * 8, dms[i * 2], dms[i * 2 + 1]);
    }

    put_rational(
        tiff + 214,
        (uint32_t)llround(fabs(alt) * 1000.0),
        1000
    );

    put_rational(
        tiff + 222,
        (uint32_t)llround(heading * 1000.0),
        1000
    );

    size_t exif_size = 6 + 230;
    size_t app1_len = exif_size + 2;

    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *out = fopen(tmp, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create temp file: %s\n", strerror(errno));
        free(jpeg);
        return 1;
    }

    unsigned char hdr[6] = {
        0xff, 0xd8,
        0xff, 0xe1,
        (unsigned char)((app1_len >> 8) & 0xff),
        (unsigned char)(app1_len & 0xff)
    };

    int failed =
        fwrite(hdr, 1, 6, out) != 6 ||
        fwrite(exif, 1, exif_size, out) != exif_size ||
        fwrite(jpeg + 2, 1, jpeg_size - 2, out) != jpeg_size - 2;

    if (fclose(out) != 0) {
        failed = 1;
    }

    free(jpeg);

    if (failed) {
        remove(tmp);
        fprintf(stderr, "Write failed\n");
        return 1;
    }

    if (rename(tmp, path) != 0) {
        remove(tmp);
        fprintf(stderr, "Rename failed\n");
        return 1;
    }

    printf("EXIF written: %s\n", path);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 7) {
        fprintf(
            stderr,
            "Usage: %s FILE TIMESTAMP LAT LON ALTITUDE HEADING\n",
            argv[0]
        );
        return 2;
    }

    return write_exif(
        argv[1],
        argv[2],
        strtod(argv[3], NULL),
        strtod(argv[4], NULL),
        strtod(argv[5], NULL),
        strtod(argv[6], NULL)
    );
}
