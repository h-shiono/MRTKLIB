#!/bin/bash
# run_rtkrcv_test.sh
# Run rtkrcv with the pre-built binary and compare output line count against
# a reference file (≥90% threshold).  Used by CTest for the rt-PPP regression.
#
# Usage:
#   bash run_rtkrcv_test.sh <rtkrcv_binary> <project_root> <reference_pos> \
#        [playback_speed] [conf_file] [port]
#
# The test:
#   1. Patches the conf file (output path, playback speed)
#   2. Runs rtkrcv with file stream replay
#   3. Waits until output stabilises (10s idle timeout, 300s max)
#   4. Checks that data line count >= 90% of reference
set -euo pipefail

RTKRCV_BIN="$1"
PROJECT_ROOT="$2"
REFERENCE="$3"
PLAYBACK_SPEED="${4:-10}"
CONF_FILE="${5:-conf/malib/rtkrcv.conf}"
RTKRCV_PORT="${6:-52003}"
MAX_TIMEOUT="${7:-300}"
IDLE_TIMEOUT=10

cd "$PROJECT_ROOT"

OUTPUT=$(mktemp /tmp/rtkrcv_ctest_XXXXXX.pos)
CONF=$(mktemp /tmp/rtkrcv_conf_XXXXXX.conf)
RTKRCV_PID=""

cleanup() {
    if [[ -n "$RTKRCV_PID" ]] && kill -0 "$RTKRCV_PID" 2>/dev/null; then
        kill -TERM "$RTKRCV_PID" 2>/dev/null || true
        wait "$RTKRCV_PID" 2>/dev/null || true
    fi
    rm -f "$OUTPUT" "$CONF" "${CONF}.bak"
}
trap cleanup EXIT

# Patch conf: set output path and playback speed
cp "$CONF_FILE" "$CONF"
sed -i.bak "s|^outstr1-path.*|outstr1-path       =${OUTPUT}|" "$CONF"
sed -i.bak "s|::x[0-9]*|::x${PLAYBACK_SPEED}|g" "$CONF"

# Start rtkrcv in background
"$RTKRCV_BIN" -s -p "$RTKRCV_PORT" -o "$CONF" &
RTKRCV_PID=$!
echo "  rtkrcv PID: $RTKRCV_PID"

# Wait for output file to appear
elapsed=0
while [[ ! -f "$OUTPUT" ]]; do
    sleep 1; elapsed=$((elapsed + 1))
    if [[ $elapsed -ge 30 ]]; then
        echo "ERROR: output file not created after 30s" >&2; exit 1
    fi
    if ! kill -0 "$RTKRCV_PID" 2>/dev/null; then
        echo "ERROR: rtkrcv exited unexpectedly" >&2; exit 1
    fi
done

# Monitor until output stabilises
prev_size=0; idle=0; total=0
while true; do
    sleep 1; total=$((total + 1))
    if ! kill -0 "$RTKRCV_PID" 2>/dev/null; then
        echo "  rtkrcv exited on its own."; break
    fi
    [[ $total -ge $MAX_TIMEOUT ]] && { echo "WARNING: max timeout reached" >&2; break; }
    curr=$(wc -c < "$OUTPUT")
    if [[ $curr -eq $prev_size ]]; then
        idle=$((idle + 1))
        [[ $idle -ge $IDLE_TIMEOUT ]] && { echo "  Output stable."; break; }
    else
        idle=0; prev_size=$curr
    fi
done

# Graceful shutdown
if kill -0 "$RTKRCV_PID" 2>/dev/null; then
    kill -TERM "$RTKRCV_PID"; wait "$RTKRCV_PID" 2>/dev/null || true
fi
RTKRCV_PID=""

# Verify output is non-empty
if [[ ! -s "$OUTPUT" ]]; then
    echo "ERROR: output file empty or missing" >&2; exit 1
fi

# Count data lines (exclude % headers)
out_lines=$(grep -v '^%' "$OUTPUT" | grep -c '[0-9]' || true)
ref_lines=$(grep -v '^%' "$REFERENCE" | grep -c '[0-9]' || true)
threshold=$(( ref_lines * 90 / 100 ))

echo "  data lines: output=$out_lines  reference=$ref_lines  threshold=$threshold"

if [[ $out_lines -lt $threshold ]]; then
    echo "ERROR: too few data lines (${out_lines} < ${threshold})" >&2; exit 1
fi

echo "OK: rtkrcv regression passed"
