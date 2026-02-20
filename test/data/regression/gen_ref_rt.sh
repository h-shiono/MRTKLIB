#!/bin/bash
set -euo pipefail

# Generate Reference Position for real-time PPP using rtkrcv
#
# Usage:
#   bash test/data/regression/gen_ref_rt.sh                # default (x10 speed)
#   bash test/data/regression/gen_ref_rt.sh --speed 20     # custom playback speed
#   bash test/data/regression/gen_ref_rt.sh --trace         # with trace (level 5)
#   bash test/data/regression/gen_ref_rt.sh --trace 3       # with trace (custom level)

# Defaults
PLAYBACK_SPEED=10
TRACE_LEVEL=0
IDLE_TIMEOUT=10       # seconds of no output growth to consider done
MAX_TIMEOUT=600       # absolute timeout (seconds)
RTKRCV_PORT=52001     # telnet port (avoids /dev/tty requirement)

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --speed)
            PLAYBACK_SPEED="${2:?--speed requires a value}"
            shift 2
            ;;
        --trace)
            if [[ "${2:-}" =~ ^[0-9]+$ ]]; then
                TRACE_LEVEL="$2"
                shift 2
            else
                TRACE_LEVEL=5
                shift
            fi
            echo "Trace enabled (level $TRACE_LEVEL)"
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Resolve project root from script location
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$ROOT_DIR"

# Output paths
output_dir=test/data/regression
output="${output_dir}/MALIB_OSS_data_obsnav_240822-1100.rt.pos"
mkdir -p "$output_dir"

# PIDs to clean up
RTKRCV_PID=""

cleanup() {
    echo "Cleaning up..."
    # Stop rtkrcv if still running
    if [[ -n "$RTKRCV_PID" ]] && kill -0 "$RTKRCV_PID" 2>/dev/null; then
        echo "  Sending SIGTERM to rtkrcv (PID $RTKRCV_PID)..."
        kill -TERM "$RTKRCV_PID" 2>/dev/null || true
        wait "$RTKRCV_PID" 2>/dev/null || true
    fi
    rm -f ./rtkrcv ./rtkrcv.conf ./rtkrcv.nav
    # Files extracted by tar
    rm -f data/2024235L.209.l6
    rm -f data/MALIB_OSS_data_obsnav_240822-1100.*
    rm -f data/MALIB_OSS_data_l6e_240822-1100.*
    rm -f data/igs14*.atx
}
trap cleanup EXIT

# Build binary
echo "Building rtkrcv..."
make -C app/consapp/rtkrcv/gcc clean
make -C app/consapp/rtkrcv/gcc
mv app/consapp/rtkrcv/gcc/rtkrcv .

# Extract test data
echo "Extracting data..."
tar -xzf data/MALIB_OSS_data.tar.gz

# Prepare config: copy and patch output path + playback speed
cp bin/rtkrcv.conf .
# Set output file path
sed -i.bak "s|^outstr1-path.*|outstr1-path       =./${output}|" rtkrcv.conf
# Set playback speed
sed -i.bak "s|::x[0-9]*|::x${PLAYBACK_SPEED}|g" rtkrcv.conf
rm -f rtkrcv.conf.bak

echo "Config: playback speed x${PLAYBACK_SPEED}, output -> ${output}"

# Build rtkrcv command
RTKRCV_CMD=(./rtkrcv -s -p "$RTKRCV_PORT" -o rtkrcv.conf)
if [[ "$TRACE_LEVEL" -gt 0 ]]; then
    RTKRCV_CMD+=(-t "$TRACE_LEVEL")
fi

# Run rtkrcv in background
echo "Starting rtkrcv (port $RTKRCV_PORT)..."
"${RTKRCV_CMD[@]}" &
RTKRCV_PID=$!
echo "  rtkrcv PID: $RTKRCV_PID"

# Wait for output file to appear
echo "Waiting for output file..."
elapsed=0
while [[ ! -f "$output" ]]; do
    sleep 1
    elapsed=$((elapsed + 1))
    if [[ $elapsed -ge 30 ]]; then
        echo "ERROR: Output file not created after 30s" >&2
        exit 1
    fi
    if ! kill -0 "$RTKRCV_PID" 2>/dev/null; then
        echo "ERROR: rtkrcv exited unexpectedly" >&2
        exit 1
    fi
done

# Monitor output file: wait until it stops growing
echo "Monitoring output (idle timeout: ${IDLE_TIMEOUT}s, max: ${MAX_TIMEOUT}s)..."
prev_size=0
idle_count=0
total_elapsed=0

while true; do
    sleep 1
    total_elapsed=$((total_elapsed + 1))

    # Check rtkrcv is still alive
    if ! kill -0 "$RTKRCV_PID" 2>/dev/null; then
        echo "  rtkrcv exited on its own."
        break
    fi

    # Check absolute timeout
    if [[ $total_elapsed -ge $MAX_TIMEOUT ]]; then
        echo "WARNING: Max timeout (${MAX_TIMEOUT}s) reached. Stopping." >&2
        break
    fi

    # Check output file size
    if [[ -f "$output" ]]; then
        curr_size=$(wc -c < "$output")
    else
        curr_size=0
    fi

    if [[ "$curr_size" -eq "$prev_size" ]]; then
        idle_count=$((idle_count + 1))
        if [[ $idle_count -ge $IDLE_TIMEOUT ]]; then
            echo "  Output stable for ${IDLE_TIMEOUT}s. Processing complete."
            break
        fi
    else
        idle_count=0
        prev_size=$curr_size
    fi

    # Progress indicator every 30s
    if [[ $((total_elapsed % 30)) -eq 0 ]]; then
        lines=$(wc -l < "$output" 2>/dev/null || echo 0)
        echo "  [${total_elapsed}s] ${lines} lines written..."
    fi
done

# Graceful shutdown
echo "Stopping rtkrcv..."
if kill -0 "$RTKRCV_PID" 2>/dev/null; then
    kill -TERM "$RTKRCV_PID"
    wait "$RTKRCV_PID" 2>/dev/null || true
fi
RTKRCV_PID=""  # prevent double-kill in cleanup

# Verify output
echo "Verifying output..."
if [[ ! -s "$output" ]]; then
    echo "ERROR: Output file not generated or empty: $output" >&2
    exit 1
fi
lines=$(wc -l < "$output")
echo "  $output ($lines lines)"

echo "Reference data generated successfully."
