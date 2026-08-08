# OpenIPC Mission Mode

OpenIPC Mission Mode adds autonomous aerial photography and survey capabilities to OpenIPC FPV cameras by combining interval photography with INAV MSP telemetry.

Image capture is controlled by an RC switch, typically the same switch used to activate an INAV Waypoint Mission. Each mission is stored in its own SD card directory and accompanied by a CSV telemetry log. Every captured JPEG is automatically geotagged with GPS position, altitude, UTC timestamp, and image heading written directly into the EXIF metadata.

The project consists of a lightweight Mission Mode controller, a standalone EXIF writer with no external dependencies, and a small modification to `msposd` that continuously exports the latest INAV telemetry for image georeferencing. This makes the captured imagery suitable for mapping, surveying, and other georeferenced aerial photography workflows.

## Tested platform

- RunCam WiFiLink 2
- OpenIPC on SigmaStar SSC338Q / Infinity6E
- INAV flight controller telemetry over `/dev/ttyS2`
- OpenIPC `msposd` source reporting `22d397e-dirty` in the tested build

## What it does

```text
INAV flight controller
        │ MSP over UART
        ▼
patched msposd
        │ latest telemetry
        ▼
/tmp/mission_telemetry.csv
        │
RC event → channels.sh → mission.sh
                           ├── JPEG snapshots
                           ├── mission.csv
                           └── mission_exif
                                  └── GPS/time/direction EXIF
```

A mission directory looks like:

```text
MISSION_20260806_143752/
├── IMG_20260806_143752_000001.jpg
├── IMG_20260806_143755_000002.jpg
├── IMG_20260806_143758_000003.jpg
└── mission.csv
```

`mission.csv` fields:

```text
filename,timestamp,latitude,longitude,altitude_m,heading_deg,speed_cmps
```

> `heading_deg` currently contains INAV `MSP_RAW_GPS` ground course. It is written to EXIF as `GPSImgDirection` with True North reference.

## Features

- RC-controlled mission start and stop
- Configurable RC channel, PWM thresholds, and capture interval
- Automatic creation of mission directories on the SD card
- JPEG image capture using the OpenIPC `/image.jpg` endpoint
- Automatic generation of a mission telemetry log (`mission.csv`)
- Automatic EXIF geotagging of every image with:
  - GPS latitude and longitude
  - GPS altitude
  - UTC `DateTimeOriginal`
  - GPS image direction (True North)
- Lightweight standalone EXIF writer with no external runtime dependencies
- Tested with RunCam WiFiLink 2 (OpenIPC) and INAV

## Quick configuration

Example `/etc/mission.conf`:

```ini
CHANNEL=8
START=1500
STOP=1000
INTERVAL=3
```

A three-position switch was tested as:

```text
1000  STOP / normal flight
1500  START / waypoint mission
2000  other mode (for example RTH; ignored by Mission Mode)
```

See [Installation](docs/installation.md) for the complete procedure.

## Verified output

Example metadata (public example coordinates):

```text
Date/Time Original              : 2026:08:06 14:37:52
GPS Latitude Ref                : North
GPS Longitude Ref               : East
GPS Altitude Ref                : Above Sea Level
GPS Img Direction Ref           : True North
GPS Img Direction               : 145
GPS Altitude                    : 120 m Above Sea Level
GPS Latitude                    : 48 deg 51' 23.76" N
GPS Longitude                   : 2 deg 21' 7.92" E
```

## Important notes

- The camera clock supplies mission folder names and EXIF time. Ensure the camera clock is synchronized before flight.
- GPS coordinates are meaningful only after INAV has a valid GPS fix.
- `/tmp/mission_telemetry.csv` always contains only the latest telemetry sample; `mission.sh` copies that sample into the mission CSV when each image is captured.
- Firmware upgrades can overwrite `/usr/bin/msposd` or `/usr/bin/wifibroadcast`; keep backups and reapply the documented changes when required.

## Repository layout

```text
src/        standalone EXIF writer
scripts/    runtime shell scripts
config/     example configuration
patches/    msposd telemetry patch
docs/       architecture, installation and verification
examples/   anonymized output examples
reference/  hashes/source snapshot from the tested camera build
```

## License

Original Mission Mode files in this repository are provided under the MIT License. The `msposd` patch modifies upstream OpenIPC code and remains subject to the upstream project's licensing terms.
