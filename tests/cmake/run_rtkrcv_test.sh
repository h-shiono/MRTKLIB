#!/bin/bash
# run_rtkrcv_test.sh
# Run rtkrcv with the pre-built binary and compare output line count against
# a reference file (≥90% threshold).  Used by CTest for the rt-PPP regression.
#
# Usage:
#   bash run_rtkrcv_test.sh <binary> <subcmd> <project_root> <reference_pos> \
#        [playback_speed] [conf_file] [port] [max_timeout] [save_output] \
#        [toml_overrides]
#
# save_output    copy the produced solution file here so a follow-up CTest
#                step can compare it (the working copy lives in /tmp and is
#                removed on exit).
# toml_overrides semicolon-separated "key=value" pairs rewritten in the temp
#                conf, e.g. "l6_merge=0".  Lets one shipped conf serve both a
#                default-settings run and a run that pins one option, without
#                keeping a near-duplicate conf in the tree.
#
# The test:
#   1. Patches the conf file (output path, playback speed, overrides)
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
SAVE_OUTPUT="${9:-}"
TOML_OVERRIDES="${10:-}"
IDLE_TIMEOUT=10

cd "$PROJECT_ROOT"

# NB: keep XXXXXX at the END of the template. BSD mktemp (macOS) only substitutes
# a trailing run of X's; a suffix after XXXXXX yields a literal, fixed path that
# collides across runs (issue #194). Append the extension by renaming afterwards.
OUTPUT=$(mktemp /tmp/rtkrcv_ctest_XXXXXX)
mv "$OUTPUT" "$OUTPUT.pos"
OUTPUT="$OUTPUT.pos"

# Determine file extension for temp conf (fallback to .conf if no '.')
CONF_BASENAME=$(basename "$CONF_FILE")
if [[ "$CONF_BASENAME" == *.* ]]; then
    CONF_EXT="${CONF_BASENAME##*.}"
else
    CONF_EXT="conf"
fi
# Same trailing-XXXXXX requirement as OUTPUT above (issue #194). The extension
# matters here: mrtk picks the TOML vs legacy parser from the conf suffix.
CONF=$(mktemp /tmp/rtkrcv_conf_XXXXXX)
mv "$CONF" "$CONF.${CONF_EXT}"
CONF="$CONF.${CONF_EXT}"
RTKRCV_PID=""

cleanup() {
    if [[ -n "$RTKRCV_PID" ]] && kill -0 "$RTKRCV_PID" 2>/dev/null; then
        kill -TERM "$RTKRCV_PID" 2>/dev/null || true
        for _ in $(seq 1 10); do
            kill -0 "$RTKRCV_PID" 2>/dev/null || break
            sleep 1
        done
        if kill -0 "$RTKRCV_PID" 2>/dev/null; then
            echo "WARNING: rtkrcv ignored SIGTERM in cleanup, sending SIGKILL" >&2
            kill -KILL "$RTKRCV_PID" 2>/dev/null || true
        fi
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
# Apply 'key=value' overrides on existing bare keys
for item in sys.argv[2].split(';'):
    if not item:
        continue
    key, _, value = item.partition('=')
    key, value = key.strip(), value.strip()
    text, n = re.subn(r'(?m)^' + re.escape(key) + r'\s*=.*$', key + ' = ' + value, text)
    if n != 1:
        sys.exit('conf override %s matched %d keys (expected 1)' % (key, n))
open(sys.argv[1], 'w').write(text)
" "$CONF" "$TOML_OVERRIDES"
else
    # Legacy .conf format
    if [[ -n "$TOML_OVERRIDES" ]]; then
        echo "ERROR: conf overrides are only supported for TOML configs" >&2; exit 1
    fi
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

# Graceful shutdown. A server that ignores SIGTERM (stuck stream or engine
# thread) must not turn into a CTest-level timeout: escalate to SIGKILL after
# 20 s so the checks below still run and report on the output produced so far.
if kill -0 "$RTKRCV_PID" 2>/dev/null; then
    kill -TERM "$RTKRCV_PID"
    for _ in $(seq 1 20); do
        kill -0 "$RTKRCV_PID" 2>/dev/null || break
        sleep 1
    done
    if kill -0 "$RTKRCV_PID" 2>/dev/null; then
        echo "WARNING: rtkrcv ignored SIGTERM, sending SIGKILL" >&2
        kill -KILL "$RTKRCV_PID" 2>/dev/null || true
    fi
    wait "$RTKRCV_PID" 2>/dev/null || true
fi
RTKRCV_PID=""

# Verify output is non-empty
if [[ ! -s "$OUTPUT" ]]; then
    echo "ERROR: output file empty or missing" >&2; exit 1
fi

# Keep the solution for follow-up comparison steps
if [[ -n "$SAVE_OUTPUT" ]]; then
    cp "$OUTPUT" "$SAVE_OUTPUT"
    echo "  saved output: $SAVE_OUTPUT"
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
