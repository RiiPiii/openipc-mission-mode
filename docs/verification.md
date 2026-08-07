# Verification

A release installation is considered working when all checks below pass after a full camera reboot, without manually starting any process.

## 1. msposd starts automatically

```sh
ps | grep msposd
```

Expected command line includes:

```text
msposd -p 500 -b 115200 -c 8 ...
```

## 2. Telemetry updates

```sh
cat /tmp/mission_telemetry.csv
```

With a valid GPS fix, latitude and longitude should be non-zero.

## 3. RC start creates a mission

Move the configured RC switch to `START`, wait several seconds, then:

```sh
ls -td /mnt/mmcblk0p1/MISSION_* | head -1
```

## 4. RC stop stops capture

Move the switch to `STOP`. The number of JPEGs should stop increasing.

## 5. CSV contains one row per captured image

Example:

```csv
filename,timestamp,latitude,longitude,altitude_m,heading_deg,speed_cmps
IMG_20260806_143752_000001.jpg,2026-08-06T14:37:52Z,48.8566000,2.3522000,120,145,166
```

## 6. EXIF contains georeferencing

On a desktop with ExifTool:

```sh
exiftool IMG_*.jpg | grep -E 'Date/Time Original|GPS Latitude|GPS Longitude|GPS Altitude|GPS Img Direction'
```

Expected fields include date/time, latitude, longitude, altitude and image direction.
