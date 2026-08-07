# Installation

## Tested platform

- RunCam WiFiLink 2
- OpenIPC on SigmaStar SSC338Q / Infinity6E
- INAV telemetry over `/dev/ttyS2`

> Back up the camera before replacing system files. Firmware updates may overwrite these changes.

## 1. Build `mission_exif`

On Linux, using the Infinity6E toolchain already downloaded by the OpenIPC `msposd` build:

```sh
make star6e STAR6E_CC="$HOME/openipc/msposd/toolchain/sigmastar-infinity6e/bin/arm-linux-gcc"
```

This creates:

```text
mission_exif_star6e
```

Copy it to the camera **under the runtime name `mission_exif`**:

```text
local:   mission_exif_star6e
camera:  /usr/bin/mission_exif
```

Then on the camera:

```sh
chmod +x /usr/bin/mission_exif
```

## 2. Patch and build `msposd`

Clone OpenIPC `msposd` and apply:

```sh
git apply patches/msposd-mission-mode.patch
./build.sh star6e
```

The tested patch does two things:

1. requests `MSP_RAW_GPS` independently of AHI display state;
2. writes the latest UTC/GPS/course/speed sample to `/tmp/mission_telemetry.csv`.

Copy the resulting `msposd_star6e` to the camera first under a temporary name and test it. When confirmed, preserve the original and replace the boot-time binary:

```sh
cp /usr/bin/msposd /usr/bin/msposd.original
cp /usr/bin/msposd_star6e /usr/bin/msposd
chmod +x /usr/bin/msposd
```

If `/usr/bin/msposd.original` already contains the known-good original, do not overwrite that backup again.

## 3. Install runtime scripts

Copy:

```text
scripts/mission.sh   → /usr/bin/mission.sh
scripts/channels.sh  → /usr/bin/channels.sh
config/mission.conf.example → /etc/mission.conf
```

Then:

```sh
chmod +x /usr/bin/mission.sh /usr/bin/channels.sh /usr/bin/mission_exif
```

## 4. Enable reliable three-position RC switching

Edit `/usr/bin/wifibroadcast` so the `msposd` launch line includes `-p 500`.

Tested line:

```sh
msposd -p 500 -b 115200 -c 8 -r "$osd_fps" -m /dev/"$serial" \
    -o 127.0.0.1:"$port_tx" -z "$size" > /dev/null &
```

The `-p 500` option allows the tested centre position (`1500`) to generate a reliable RC event after remaining stable for 500 ms.

> The `-c 8` value is the channel monitored by `msposd`. It must match the `CHANNEL=` value in `/etc/mission.conf`.

## 5. Configure Mission Mode

Example `/etc/mission.conf`:

```ini
CHANNEL=8
START=1500
STOP=1000
INTERVAL=3
```

- `CHANNEL`: monitored RC channel
- `START`: PWM value that starts image capture
- `STOP`: PWM value that stops image capture
- `INTERVAL`: seconds between snapshots

## 6. Reboot and verify

After reboot:

```sh
ps | grep msposd
```

The command line should contain:

```text
-p 500 -b 115200 -c 8
```

Check telemetry:

```sh
cat /tmp/mission_telemetry.csv
```

Example:

```text
2026-08-06T14:37:52Z,48.8566000,2.3522000,120,145,166
```

Move the RC switch to `START`. A new directory should appear on the SD card:

```sh
ls -td /mnt/mmcblk0p1/MISSION_* | head -1
```

Move the switch to `STOP`, then inspect the directory for JPEGs and `mission.csv`.

See [Verification](verification.md) for the final checks.
