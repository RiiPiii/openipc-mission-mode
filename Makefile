CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
LDLIBS ?= -lm
STAR6E_CC ?= $(HOME)/openipc/msposd/toolchain/sigmastar-infinity6e/bin/arm-linux-gcc

.PHONY: all host star6e clean

all: host

host: mission_exif

mission_exif: src/mission_exif.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

star6e: mission_exif_star6e mission_image_detect_star6e

mission_exif_star6e: src/mission_exif.c
	$(STAR6E_CC) $(CFLAGS) $< $(LDLIBS) -o $@

mission_image_detect_star6e: src/mission_image_detect.c
	$(STAR6E_CC) $(CFLAGS) \
		-Iinclude -Isrc \
		$< -ldl -lm \
		-o $@

clean:
	rm -f mission_exif mission_exif_star6e mission_image_detect_star6e
