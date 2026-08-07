# Tested camera reference hashes

These hashes were recorded from the working RunCam WiFiLink 2 installation before preparing the public source package.

```text
77bc49afa969e94a6ef5aca8336253d3  /usr/bin/msposd       (patched, camera-tested)
a36e6e45d2b2b4e61463cd3915eaddc5  /usr/bin/msposd.original
058fa1d877e0d67537db11d0e5489d51  /usr/bin/mission_exif (camera-tested build)
```

The public `src/mission_exif.c` contains a final EXIF-layout correction found during repository validation. Rebuild `mission_exif` from source for new installations.
