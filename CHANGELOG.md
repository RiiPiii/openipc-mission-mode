# Changelog

## 1.0.0

- RC-controlled Mission Mode for OpenIPC
- configurable RC channel, start/stop PWM values and capture interval
- per-mission JPEG folders and `mission.csv`
- INAV `MSP_RAW_GPS` telemetry export from patched `msposd`
- standalone JPEG EXIF writer with no external runtime dependency
- EXIF GPS position, altitude, UTC capture time and image direction
- tested reboot persistence on RunCam WiFiLink 2 / SSC338Q
- reliable three-position switch centre detection using `msposd -p 500`
