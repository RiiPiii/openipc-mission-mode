#!/bin/sh

CONF=/etc/mission.conf
BASE=/mnt/mmcblk0p1
PIDFILE=/tmp/mission.pid
TELEMETRY=/tmp/mission_telemetry.csv

[ -f "$CONF" ] || exit 1
. "$CONF"

start_mission() {
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        exit 0
    fi

    DIR="$BASE/MISSION_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$DIR"

    CSV="$DIR/mission.csv"
    echo "filename,timestamp,latitude,longitude,altitude_m,heading_deg,speed_cmps" > "$CSV"

    (
        N=1

        while true; do
            TIMESTAMP=$(date +%Y%m%d_%H%M%S)
            NAME=$(printf "IMG_%s_%06d.jpg" "$TIMESTAMP" "$N")
            FILE="$DIR/$NAME"

            if wget -q -O "$FILE" http://127.0.0.1/image.jpg; then
                if [ -s "$TELEMETRY" ]; then
                    DATA=$(cat "$TELEMETRY")
                    echo "$NAME,$DATA" >> "$CSV"

                    TS=$(echo "$DATA" | cut -d, -f1)
                    LAT=$(echo "$DATA" | cut -d, -f2)
                    LON=$(echo "$DATA" | cut -d, -f3)
                    ALT=$(echo "$DATA" | cut -d, -f4)
                    HDG=$(echo "$DATA" | cut -d, -f5)

                    /usr/bin/mission_exif "$FILE" "$TS" "$LAT" "$LON" "$ALT" "$HDG"
                fi
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
