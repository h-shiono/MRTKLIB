#!/bin/bash
set -euo pipefail

# Generate Reference Position for MADOCA-PPP (madocalib) using rnx2rtkp
#
# This script documents how the golden reference data was generated.
# It uses MRTKLIB's rnx2rtkp with the MADOCALIB PPP engine to process
# MIZU station data from 2025/04/01 Session A (00:00-00:59 UTC).
#
# Prerequisites:
#   1. cmake --preset default && cmake --build build
#   2. Test data archive must exist at tests/data/madocalib/madocalib_testdata.tar.gz
#      (see "Test Data Preparation" section below)
#
# Usage:
#   bash tests/data/madocalib/generate_reference_madocalib.sh            # without trace
#   bash tests/data/madocalib/generate_reference_madocalib.sh --trace     # with trace (level 5)
#   bash tests/data/madocalib/generate_reference_madocalib.sh --trace 3   # with trace (custom level)
#
# Test Data Preparation:
#   The test data archive contains a 1-hour subset of madocalib sample_data:
#     - MIZU00JPN_R_20250910000_01D_30S_MO.obs  (MIZU RINEX obs, 00:00-00:59)
#     - BRDM00DLR_S_20250910000_01D_MN.rnx      (broadcast nav, full day)
#     - 2025091A.204.l6                          (L6E MADOCA-PPP E1, session A)
#     - 2025091A.206.l6                          (L6E MADOCA-PPP E2, session A)
#     - igs20.atx                                (IGS20 antenna calibration)
#
#   Source: upstream/madocalib/sample_data/data/
#   OBS file was trimmed to 1 hour (00:00:00-00:59:30 GPST) from the full-day file.
#   To create the archive (run from project root):
#     cd tests/data/madocalib
#     tar czf madocalib_testdata.tar.gz \
#         MIZU00JPN_R_20250910000_01D_30S_MO.obs \
#         BRDM00DLR_S_20250910000_01D_MN.rnx \
#         2025091A.204.l6 \
#         2025091A.206.l6 \
#         igs20.atx

# Parse options
TRACE_OPTS=()
if [[ "${1:-}" == "--trace" ]]; then
    level="${2:-5}"
    TRACE_OPTS=(-x "$level")
    echo "Trace enabled (level $level)"
fi

# Resolve project root from script location
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$ROOT_DIR"

# Locate cmake-built binary
RNX2RTKP="${ROOT_DIR}/build/rnx2rtkp"
if [[ ! -x "$RNX2RTKP" ]]; then
    echo "ERROR: $RNX2RTKP not found. Run 'cmake --preset default && cmake --build build' first." >&2
    exit 1
fi
echo "Using binary: $RNX2RTKP"

# Test data directory
DATADIR=tests/data/madocalib

# Temporary files to clean up (populated as script runs)
CLEANUP_FILES=()

cleanup() {
    echo "Cleaning up..."
    for f in ${CLEANUP_FILES[@]+"${CLEANUP_FILES[@]}"}; do
        rm -f "$f"
    done
    # Files extracted by tar
    rm -f "${DATADIR}/MIZU00JPN_R_20250910000_01D_30S_MO.obs"
    rm -f "${DATADIR}/BRDM00DLR_S_20250910000_01D_MN.rnx"
    rm -f "${DATADIR}/2025091A.204.l6"
    rm -f "${DATADIR}/2025091A.206.l6"
    rm -f "${DATADIR}/igs20.atx"
}
trap cleanup EXIT

# Extract test data
ARCHIVE="${DATADIR}/madocalib_testdata.tar.gz"
if [[ ! -f "$ARCHIVE" ]]; then
    echo "ERROR: Test data archive not found: $ARCHIVE" >&2
    echo "See 'Test Data Preparation' in this script header." >&2
    exit 1
fi
echo "Extracting data..."
tar -xzf "$ARCHIVE" -C "$DATADIR"

# Input files
obs="${DATADIR}/MIZU00JPN_R_20250910000_01D_30S_MO.obs"
nav="${DATADIR}/BRDM00DLR_S_20250910000_01D_MN.rnx"
l6e1="${DATADIR}/2025091A.204.l6"
l6e2="${DATADIR}/2025091A.206.l6"

# Verify inputs exist
for f in "$obs" "$nav" "$l6e1" "$l6e2"; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: Input file not found: $f" >&2
        exit 1
    fi
done

# Output directory
output_dir="${DATADIR}"

# Execute rnx2rtkp (MADOCA-PPP)
output="${output_dir}/madocalib_MIZU_20250401-0000.pp.pos"
echo "Running rnx2rtkp for MADOCA-PPP (Session A: 00:00-00:59)..."
"$RNX2RTKP" \
    -k conf/madocalib/rnx2rtkp.conf \
    -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30 \
    ${TRACE_OPTS[@]+"${TRACE_OPTS[@]}"} \
    -o "$output" \
    "$obs" "$nav" "$l6e1" "$l6e2"

# Verify output
echo "Verifying output..."
if [[ ! -s "$output" ]]; then
    echo "ERROR: Output file not generated or empty: $output" >&2
    exit 1
fi
lines=$(wc -l < "$output")
echo "  $output ($lines lines)"

echo "Reference data generated successfully."
