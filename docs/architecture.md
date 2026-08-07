# Architecture

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
                           └── mission_exif → GPS/time/direction EXIF
```

`msposd` only exports the latest telemetry. `mission.sh` owns mission state, folder creation and image capture. `mission_exif` only writes GPS position, altitude, UTC timestamp and image direction into JPEG EXIF metadata.

## Telemetry flow

The patch makes `msposd` request `MSP_RAW_GPS` even when graphical AHI is disabled. Each received packet updates:

```text
/tmp/mission_telemetry.csv
```

The current format is:

```text
timestamp,latitude,longitude,altitude_m,ground_course_deg,speed_cmps
```

The file is replaced atomically through a `.tmp` file so readers see one complete sample.

## RC flow

`msposd` monitors the configured RC channel and invokes:

```text
/usr/bin/channels.sh CHANNEL PWM
```

`channels.sh` forwards the event to `mission.sh`, which compares it with `/etc/mission.conf`.

`-p 500` is required in the tested setup so the centre position of a three-position switch reliably generates an event after it remains stable for 500 ms.

## Image flow

For every interval:

1. `mission.sh` downloads `http://127.0.0.1/image.jpg`.
2. The current telemetry sample is appended to `mission.csv` with the JPEG filename.
3. `mission_exif` writes position, altitude, time and direction into the JPEG.
