#!/bin/sh

CONF=/etc/mission.conf
BASE=/mnt/mmcblk0p1
PIDFILE=/tmp/mission.pid
TELEMETRY=/tmp/mission_telemetry.csv
DETECTOR=/tmp/mission_image_detect

[ -f "$CONF" ] || exit 1
. "$CONF"

# INTERVAL võib muuta, kuid mitte alla 10 sekundi.
case "$INTERVAL" in
    ''|*[!0-9]*) INTERVAL=25 ;;
esac
[ "$INTERVAL" -lt 10 ] && INTERVAL=10

start_mission() {
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        exit 0
    fi

    DIR="$BASE/MISSION_$(date +%Y%m%d_%H%M%S)"
    DETECTED="$DIR/detected"
    CLEAR="$DIR/clear"

    mkdir -p "$DETECTED" "$CLEAR"

    CSV="$DIR/mission.csv"
    echo "filename,timestamp,latitude,longitude,altitude_m,heading_deg,speed_cmps,ai_detected" > "$CSV"

    (
        N=1

        while true; do
            TIMESTAMP=$(date +%Y%m%d_%H%M%S)
            NAME=$(printf "IMG_%s_%06d.jpg" "$TIMESTAMP" "$N")

            # Kõigepealt salvestame täpselt selle JPG, mida AI analüüsib.
            TMPFILE="$DIR/$NAME"

            if wget -q -O "$TMPFILE" http://127.0.0.1/image.jpg && [ -s "$TMPFILE" ]; then

                LD_LIBRARY_PATH=/tmp:/usr/lib:/lib \
                LD_PRELOAD="/tmp/libcus3a.so:/tmp/libispalgo.so:/tmp/libmi_isp.so" \
                "$DETECTOR" "$TMPFILE" >/tmp/mission_image_detect.log 2>&1

                AI_RESULT=$?

                if [ "$AI_RESULT" -eq 1 ]; then
                    AI=1
                    RELFILE="detected/$NAME"
                    FILE="$DETECTED/$NAME"
                elif [ "$AI_RESULT" -eq 0 ]; then
                    AI=0
                    RELFILE="clear/$NAME"
                    FILE="$CLEAR/$NAME"
                else
                    # AI vea korral ei märgi pilti ekslikult CLEAR-iks.
                    # Jätame selle missiooni juurkausta.
                    AI=-1
                    RELFILE="$NAME"
                    FILE="$TMPFILE"
                fi

                if [ "$AI_RESULT" -eq 0 ] || [ "$AI_RESULT" -eq 1 ]; then
                    mv "$TMPFILE" "$FILE"
                fi

                if [ -s "$TELEMETRY" ]; then
                    DATA=$(cat "$TELEMETRY")

                    TS=$(echo "$DATA" | cut -d, -f1)
                    LAT=$(echo "$DATA" | cut -d, -f2)
                    LON=$(echo "$DATA" | cut -d, -f3)
                    ALT=$(echo "$DATA" | cut -d, -f4)
                    HDG=$(echo "$DATA" | cut -d, -f5)

                    /usr/bin/mission_exif \
                        "$FILE" "$TS" "$LAT" "$LON" "$ALT" "$HDG"

                    echo "$RELFILE,$DATA,$AI" >> "$CSV"
                else
                    echo "$RELFILE,,,,,,,$AI" >> "$CSV"
                fi

                echo "MISSION: $RELFILE AI=$AI"
            fi

            N=$((N + 1))
            sleep "$INTERVAL"
        done
    ) &

    echo $! > "$PIDFILE"
}

stop_mission() {
    if [ -f "$PIDFILE" ]; then
        kill "$(cat "$PIDFILE")" 2>/dev/null
        rm -f "$PIDFILE"
    fi
}

RC_CHANNEL="$1"
RC_VALUE="$2"

[ "$RC_CHANNEL" = "$CHANNEL" ] || exit 0

if [ "$RC_VALUE" = "$START" ]; then
    start_mission
elif [ "$RC_VALUE" = "$STOP" ]; then
    stop_mission
fi
