#!/bin/bash
# run_rtkrcv_test.sh
# Run rtkrcv with the pre-built binary and compare output line count against
# a reference file (≥90% threshold).  Used by CTest for the rt-PPP regression.
#
# Usage:
#   bash run_rtkrcv_test.sh <binary> <subcmd> <project_root> <reference_pos> \
#        [playback_speed] [conf_file] [port]
#
# The test:
#   1. Patches the conf file (output path, playback speed)
#   2. Runs rtkrcv with file stream replay
#   3. Waits until output stabilises (10s idle timeout, 300s max)
#   4. Checks that data line count >= 90% of reference
set -euo pipefail

RTKRCV_BIN="$1"
RTKRCV_SUBCMD="$2"
PROJECT_ROOT="$3"
REFERENCE="$4"
PLAYBACK_SPEED="${5:-10}"
CONF_FILE="${6:-conf/malib/rtkrcv.toml}"
RTKRCV_PORT="${7:-52003}"
MAX_TIMEOUT="${8:-300}"
IDLE_TIMEOUT=10

cd "$PROJECT_ROOT"

OUTPUT=$(mktemp /tmp/rtkrcv_ctest_XXXXXX.pos)

# Determine file extension for temp conf (fallback to .conf if no '.')
CONF_BASENAME=$(basename "$CONF_FILE")
if [[ "$CONF_BASENAME" == *.* ]]; then
    CONF_EXT="${CONF_BASENAME##*.}"
else
    CONF_EXT="conf"
fi
CONF=$(mktemp /tmp/rtkrcv_conf_XXXXXX."${CONF_EXT}")
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
if [[ "$CONF_EXT" == "toml" ]]; then
    # TOML format: patch path under [streams.output.stream1]
    # Replace the outstr1 path value (line matching 'path' after output.stream1 section)
    python3 -c "
import sys, re
text = open(sys.argv[1]).read()
# Replace output stream1 path
text = re.sub(
    r'(\[streams\.output\.stream1\][^\[]*?path\s*=\s*)\"[^\"]*\"',
    r'\1\"${OUTPUT}\"',
    text, count=1, flags=re.DOTALL)
# Replace playback speed
text = re.sub(r'::x[0-9]+', '::x${PLAYBACK_SPEED}', text)
open(sys.argv[1], 'w').write(text)
" "$CONF"
else
    # Legacy .conf format
    sed -i.bak "s|^outstr1-path.*|outstr1-path       =${OUTPUT}|" "$CONF"
    sed -i.bak "s|::x[0-9]*|::x${PLAYBACK_SPEED}|g" "$CONF"
fi

# Start rtkrcv in background
"$RTKRCV_BIN" "$RTKRCV_SUBCMD" -s -p "$RTKRCV_PORT" -o "$CONF" &
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
