# OpenIPC Mission Mode

OpenIPC Mission Mode adds autonomous aerial photography, georeferencing, and on-camera AI object detection to OpenIPC FPV cameras.

Image capture is controlled by an RC switch, typically the same switch used to activate an INAV Waypoint Mission. During an active mission the camera captures JPEG images at a configurable interval, records the corresponding INAV telemetry, writes GPS/time/direction information into EXIF metadata, and classifies each captured image using the SigmaStar IPU.

The AI analyzes the **same JPEG image that Mission Mode saves**.

## Tested platform

- RunCam WiFiLink 2
- OpenIPC on SigmaStar SSC338Q / Infinity6E
- INAV telemetry over `/dev/ttyS2`
- OpenIPC `msposd`
- SigmaStar IPU
- YOLOv8 640x352 NV12 model
- SD card mounted at `/mnt/mmcblk0p1`

## Mission flow

```text
INAV → MSP → msposd → /tmp/mission_telemetry.csv
                         │
RC event → channels.sh → mission.sh
                         ├── JPEG from /image.jpg
                         ├── mission_image_detect
                         │      ├── clear/
                         │      └── detected/
                         ├── mission_exif
                         └── mission.csv
```

## AI image classification

Mission Mode analyzes each interval photograph using the SigmaStar hardware IPU and a YOLO model. The same JPEG that is saved for the mission is passed to the detector.

```text
AI result 0  → clear/
AI result 1  → detected/
AI error     → mission root directory
```

Detected images are annotated with AI detection information. Clear images are stored without AI annotations. If the detector returns an error, the image is not incorrectly classified as clear; it remains in the mission root and is logged with `ai_detected=-1`.

## Mission directory

```text
MISSION_20260812_143752/
├── detected/
│   ├── IMG_20260812_143817_000002.jpg
│   └── IMG_20260812_143842_000003.jpg
├── clear/
│   └── IMG_20260812_143752_000001.jpg
└── mission.csv
```

`mission.csv` fields:

```text
filename,timestamp,latitude,longitude,altitude_m,heading_deg,speed_cmps,ai_detected
```

`ai_detected`: `0` = clear, `1` = detected, `-1` = detector error.

> `heading_deg` currently contains INAV `MSP_RAW_GPS` ground course. It is written to EXIF as `GPSImgDirection` with True North reference.

## Features

- RC-controlled mission start and stop
- Configurable RC channel, PWM values, and capture interval
- Minimum capture interval enforced at 10 seconds
- Default example interval of 25 seconds
- Automatic mission directories on the SD card
- JPEG capture using OpenIPC `/image.jpg`
- AI analysis of the exact JPEG captured by Mission Mode
- SigmaStar hardware-IPU inference
- Automatic sorting into `detected/` and `clear/`
- AI result in `mission.csv`
- Automatic telemetry logging
- Automatic EXIF GPS, altitude, UTC time, and image direction
- Restart-persistent AI runtime
- OSD and AI tested together

## Quick configuration

Example `/etc/mission.conf`:

```ini
CHANNEL=8
START=1500
STOP=1000
INTERVAL=25
```

A three-position switch was tested as:

```text
1000  STOP / normal flight
1500  START / waypoint mission
2000  other mode (for example RTH; ignored by Mission Mode)
```

See [Installation](docs/installation.md) for the complete installation procedure.

## OSD and saved photographs

Mission Mode obtains JPEGs from `http://127.0.0.1/image.jpg`. The normal OpenIPC camera setting therefore determines whether OSD information is included in saved mission photographs.

```text
snapshot OSD enabled  → normal OSD visible in mission photographs
snapshot OSD disabled → normal OSD absent from mission photographs
```

AI annotations on detected photographs are independent of this setting.

The included `libipu_yolo_worker.so` is the tested no-RGN worker variant. During testing, the RGN-enabled worker interfered with `msposd`; the no-RGN worker allowed AI inference and the normal OSD to operate together.

## AI runtime and reboot persistence

Persistent AI runtime files are stored on the SD card at:

```text
/mnt/mmcblk0p1/mission-ai/
```

The tested runtime contains:

```text
mission_image_detect
libmission_detect.so
libipu_yolo_worker.so
libmi_ipu.so
libcam_fs_wrapper.so
libcus3a.so
libispalgo.so
libmi_isp.so
mi_ipu.ko
yolov8n352drone.img
```

At boot, the tested `mission-rc.local` logic waits for normal OpenIPC startup, restores `msposd` through `wifibroadcast start` if needed, copies the AI runtime from the SD card to `/tmp`, loads `mi_ipu.ko` when necessary, and creates `/dev/mi/ipu -> /dev/mi_ipu`.

## Building

For the tested SigmaStar Infinity6E target:

```bash
make star6e
```

This builds:

```text
mission_exif_star6e
mission_image_detect_star6e
```

The default cross-compiler path is:

```text
~/openipc/msposd/toolchain/sigmastar-infinity6e/bin/arm-linux-gcc
```

It can be overridden with `STAR6E_CC`.

## Repository layout

```text
src/             Mission Mode C sources and image helpers
include/         detector and SigmaStar ABI headers
scripts/         runtime shell scripts and rc.local startup script
config/          example configuration
runtime/star6e/  tested SigmaStar AI runtime, IPU module and YOLO model
patches/         msposd telemetry patch
docs/            architecture, installation and verification
examples/        anonymized output examples
reference/       hashes/source snapshot from the tested camera build
```

## Important notes

- Synchronize the camera clock before flight.
- GPS coordinates are meaningful only after INAV has a valid GPS fix.
- `/tmp/mission_telemetry.csv` contains the latest telemetry sample used when each image is captured.
- AI performance and confidence thresholds should be evaluated with representative aerial imagery.
- Firmware upgrades can overwrite Mission Mode modifications or installed files.
- The tested AI runtime is specific to the SigmaStar SSC338Q / Infinity6E platform used by the RunCam WiFiLink 2.

## License

Original Mission Mode files in this repository are provided under the MIT License.

The `msposd` patch modifies upstream OpenIPC code and remains subject to the upstream project's licensing terms. Third-party headers, libraries, kernel modules, model files, and other runtime components remain subject to their respective upstream licenses.
