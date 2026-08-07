# Reference files

The public source package was prepared from files downloaded directly from the working camera and from the Linux build tree used to compile it.

`mission_exif-camera-tested-source.c` is the exact source snapshot downloaded during final verification. During repository review, its hand-built TIFF/EXIF layout was found to overlap the final bytes of `DateTimeOriginal` and to omit `GPSImgDirection` from the declared GPS IFD entry count. The public `src/mission_exif.c` fixes those layout offsets while preserving the same command-line interface.

The exact camera hashes are recorded in `WORKING-HASHES.md`.
