#!/bin/sh
# Launcher for InkRSS (RSS reader with Miniflux support). The binary and its data live under
# system/, which the reader's library does not index. stdout/stderr go next to the log.
DIR=/mnt/ext1/system/inkrss
mkdir -p "$DIR"
# Qt warnings go here; keep it from growing forever (the app rotates log.txt itself).
if [ -f "$DIR/stderr.txt" ] && [ "$(wc -c < "$DIR/stderr.txt")" -gt 1048576 ]; then
    mv -f "$DIR/stderr.txt" "$DIR/stderr.1.txt"
fi
echo "=== launch $(date) ===" >> "$DIR/stderr.txt"
export LD_LIBRARY_PATH=/ebrmain/lib:/usr/lib:/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec "$DIR/bin/inkrss" >> "$DIR/stderr.txt" 2>&1
