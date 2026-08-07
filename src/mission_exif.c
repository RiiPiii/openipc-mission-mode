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

    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &y, &m, &d, &hh, &mm, &ss) != 6)
        return -1;

    if (y < 0 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31 ||
        hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 60)
        return -1;

    snprintf(out, 20, "%04d:%02d:%02d %02d:%02d:%02d", y, m, d, hh, mm, ss);
    return 0;
}

static void write_ifd_entry(unsigned char *entry, uint16_t tag, uint16_t type,
                            uint32_t count, uint32_t value_or_offset) {
    put16le(entry + 0, tag);
    put16le(entry + 2, type);
    put32le(entry + 4, count);
    put32le(entry + 8, value_or_offset);
}

static int write_exif(const char *path, const char *timestamp,
                      double lat, double lon, double alt, double heading) {
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

    while (heading < 0.0) heading += 360.0;
    while (heading >= 360.0) heading -= 360.0;

    enum {
        TIFF_IFD0_OFFSET = 8,
        TIFF_EXIF_IFD_OFFSET = 38,
        TIFF_DATETIME_OFFSET = 56,
        TIFF_GPS_IFD_OFFSET = 76,
        TIFF_GPS_LAT_OFFSET = 190,
        TIFF_GPS_LON_OFFSET = 214,
        TIFF_GPS_ALT_OFFSET = 238,
        TIFF_GPS_DIR_OFFSET = 246,
        TIFF_SIZE = 254,
        EXIF_SIZE = 6 + TIFF_SIZE
    };

    unsigned char exif[EXIF_SIZE];
    memset(exif, 0, sizeof(exif));
    memcpy(exif, "Exif\0\0", 6);

    unsigned char *tiff = exif + 6;
    memcpy(tiff, "II", 2);
    put16le(tiff + 2, 42);
    put32le(tiff + 4, TIFF_IFD0_OFFSET);

    /* IFD0: ExifIFDPointer + GPSInfoIFDPointer */
    unsigned char *ifd0 = tiff + TIFF_IFD0_OFFSET;
    put16le(ifd0, 2);
    write_ifd_entry(ifd0 + 2, 0x8769, 4, 1, TIFF_EXIF_IFD_OFFSET);
    write_ifd_entry(ifd0 + 14, 0x8825, 4, 1, TIFF_GPS_IFD_OFFSET);
    put32le(ifd0 + 26, 0);

    /* Exif IFD: DateTimeOriginal */
    unsigned char *exif_ifd = tiff + TIFF_EXIF_IFD_OFFSET;
    put16le(exif_ifd, 1);
    write_ifd_entry(exif_ifd + 2, 0x9003, 2, 20, TIFF_DATETIME_OFFSET);
    put32le(exif_ifd + 14, 0);
    memcpy(tiff + TIFF_DATETIME_OFFSET, exif_time, 19);
    tiff[TIFF_DATETIME_OFFSET + 19] = '\0';

    /* GPS IFD: version, latitude, longitude, altitude and image direction. */
    unsigned char *gps = tiff + TIFF_GPS_IFD_OFFSET;
    put16le(gps, 9);
    unsigned char *g = gps + 2;

    write_ifd_entry(g, 0x0000, 1, 4, 0);
    g[8] = 2; g[9] = 3; g[10] = 0; g[11] = 0;
    g += 12;

    write_ifd_entry(g, 0x0001, 2, 2, 0);
    g[8] = lat >= 0.0 ? 'N' : 'S'; g[9] = '\0';
    g += 12;

    write_ifd_entry(g, 0x0002, 5, 3, TIFF_GPS_LAT_OFFSET);
    g += 12;

    write_ifd_entry(g, 0x0003, 2, 2, 0);
    g[8] = lon >= 0.0 ? 'E' : 'W'; g[9] = '\0';
    g += 12;

    write_ifd_entry(g, 0x0004, 5, 3, TIFF_GPS_LON_OFFSET);
    g += 12;

    write_ifd_entry(g, 0x0005, 1, 1, 0);
    g[8] = alt < 0.0 ? 1 : 0;
    g += 12;

    write_ifd_entry(g, 0x0006, 5, 1, TIFF_GPS_ALT_OFFSET);
    g += 12;

    write_ifd_entry(g, 0x0010, 2, 2, 0);
    g[8] = 'T'; g[9] = '\0';
    g += 12;

    write_ifd_entry(g, 0x0011, 5, 1, TIFF_GPS_DIR_OFFSET);
    put32le(gps + 2 + 9 * 12, 0);

    uint32_t dms[6];
    decimal_to_dms(lat, dms);
    for (int i = 0; i < 3; i++)
        put_rational(tiff + TIFF_GPS_LAT_OFFSET + i * 8, dms[i * 2], dms[i * 2 + 1]);

    decimal_to_dms(lon, dms);
    for (int i = 0; i < 3; i++)
        put_rational(tiff + TIFF_GPS_LON_OFFSET + i * 8, dms[i * 2], dms[i * 2 + 1]);

    put_rational(tiff + TIFF_GPS_ALT_OFFSET,
                 (uint32_t)llround(fabs(alt) * 1000.0), 1000);
    put_rational(tiff + TIFF_GPS_DIR_OFFSET,
                 (uint32_t)llround(heading * 1000.0), 1000);

    size_t app1_len = EXIF_SIZE + 2;
    char tmp[4096];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
        fprintf(stderr, "Path too long\n");
        free(jpeg);
        return 1;
    }

    FILE *out = fopen(tmp, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create temp file: %s\n", strerror(errno));
        free(jpeg);
        return 1;
    }

    unsigned char hdr[6] = {
        0xff, 0xd8, 0xff, 0xe1,
        (unsigned char)((app1_len >> 8) & 0xff),
        (unsigned char)(app1_len & 0xff)
    };

    int failed =
        fwrite(hdr, 1, sizeof(hdr), out) != sizeof(hdr) ||
        fwrite(exif, 1, EXIF_SIZE, out) != EXIF_SIZE ||
        fwrite(jpeg + 2, 1, jpeg_size - 2, out) != jpeg_size - 2;

    if (fclose(out) != 0) failed = 1;
    free(jpeg);

    if (failed) {
        remove(tmp);
        fprintf(stderr, "Write failed\n");
        return 1;
    }

    if (rename(tmp, path) != 0) {
        remove(tmp);
        fprintf(stderr, "Rename failed: %s\n", strerror(errno));
        return 1;
    }

    printf("EXIF written: %s\n", path);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s FILE TIMESTAMP LAT LON ALTITUDE DIRECTION\n", argv[0]);
        return 2;
    }

    return write_exif(argv[1], argv[2], strtod(argv[3], NULL), strtod(argv[4], NULL),
                      strtod(argv[5], NULL), strtod(argv[6], NULL));
}
