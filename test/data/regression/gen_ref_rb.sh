#!/bin/bash
set -euo pipefail

# Generate Reference Receiver Code Biases using recvbias
#
# This script:
#   1. Builds recvbias from source
#   2. Extracts test data from data/MALIB_OSS_data.tar.gz
#   3. Downloads IONEX TEC data from CODE (Univ. of Bern) if not already present
#   4. Runs recvbias to generate reference receiver code biases
#
# The TEC file is downloaded to data/ and cleaned up on exit.
#
# Usage:
#   bash test/data/regression/gen_ref_rb.sh            # without trace
#   bash test/data/regression/gen_ref_rb.sh --trace     # with trace (level 5)
#   bash test/data/regression/gen_ref_rb.sh --trace 3   # with trace (custom level)
#
# Requirements:
#   - curl (for downloading IONEX TEC file)
#   - gunzip (for decompressing .gz file)

# Parse options
TRACE_OPTS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --trace)
            if [[ "${2:-}" =~ ^[0-9]+$ ]]; then
                level="$2"
                shift 2
            else
                level=5
                shift
            fi
            TRACE_OPTS=(-t "$level")
            echo "Trace enabled (level $level)"
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

# Test parameters
#   Date: 2024-08-22 (DOY 235, GPS week 2328)
#   ECEF position computed from PPP reference output:
#     lat=36.068735658, lon=140.128353589, h=113.288m (WGS84)
DATE="2024/8/22"
ECEF_POS="-3961440.433,3308948.812,3734425.963"
EL_MASK=30

# IONEX TEC file (CODE analysis center, University of Bern)
#   Product: CODE final global ionosphere map (long-name convention)
#   Format:  IONEX 1.0
#   .INX = IONEX TEC grid data (not .ION which is spherical harmonic coefficients)
TEC_BASENAME="COD0OPSFIN_20242350000_01D_01H_GIM.INX"
TEC_FILE="data/${TEC_BASENAME}"
TEC_URL="http://ftp.aiub.unibe.ch/CODE/2024/${TEC_BASENAME}.gz"

# Temporary files to clean up (populated as script runs)
CLEANUP_FILES=()

cleanup() {
    echo "Cleaning up..."
    for f in ${CLEANUP_FILES[@]+"${CLEANUP_FILES[@]}"}; do
        rm -f "$f"
    done
    # Files extracted by tar and downloaded TEC file
    rm -f data/2024235L.209.l6
    rm -f data/MALIB_OSS_data_obsnav_240822-1100.*
    rm -f data/MALIB_OSS_data_l6e_240822-1100.*
    rm -f data/igs14*.atx
    rm -f "data/${TEC_BASENAME}"
}
trap cleanup EXIT

# Build binary
echo "Building recvbias..."
make -C app/consapp/recvbias/gcc clean
make -C app/consapp/recvbias/gcc
mv app/consapp/recvbias/gcc/recvbias .
CLEANUP_FILES+=(./recvbias)

# Extract test data
echo "Extracting data..."
tar -xzf data/MALIB_OSS_data.tar.gz

# Download IONEX TEC file if not present in extracted data
if [[ ! -f "$TEC_FILE" ]]; then
    echo "Downloading IONEX TEC file from CODE..."
    echo "  URL: $TEC_URL"
    tec_compressed="${TEC_FILE}.gz"
    if curl -fSL -o "$tec_compressed" "$TEC_URL"; then
        # Use gzip -dc to avoid "has other links" error on APFS (macOS)
        gzip -dc "$tec_compressed" > "$TEC_FILE"
        rm -f "$tec_compressed"
        echo "  Downloaded: $TEC_FILE"
    else
        rm -f "$tec_compressed"
        echo "ERROR: Failed to download IONEX TEC file" >&2
        echo "  URL: $TEC_URL" >&2
        echo "  Please manually download and place at: $TEC_FILE" >&2
        exit 1
    fi
else
    echo "IONEX TEC file already present: $TEC_FILE"
fi

# Input files
obs=data/MALIB_OSS_data_obsnav_240822-1100.obs
nav=data/MALIB_OSS_data_obsnav_240822-1100.nav
l6e=data/2024235L.209.l6

# Output directory
output_dir=test/data/regression
mkdir -p "$output_dir"

# Execute recvbias
# Note: recvbias returns exit code 1 on success (non-standard C convention).
#       Exit code 255 (-1 wrapped) indicates actual failure.
output="${output_dir}/MALIB_OSS_data_obsnav_240822-1100.rb.bia"
echo "Running recvbias..."
set +e
./recvbias \
    -td "$DATE" \
    -nav "$nav" \
    -tec "$TEC_FILE" \
    -scb "$l6e" \
    -pos "$ECEF_POS" \
    -el "$EL_MASK" \
    ${TRACE_OPTS[@]+"${TRACE_OPTS[@]}"} \
    -out "$output" \
    "$obs"
rc=$?
set -e

if [[ $rc -ne 0 && $rc -ne 1 ]]; then
    echo "ERROR: recvbias failed (exit code $rc)" >&2
    exit 1
fi

# Verify output
echo "Verifying output..."
if [[ ! -s "$output" ]]; then
    echo "ERROR: Output file not generated or empty: $output" >&2
    exit 1
fi
lines=$(wc -l < "$output")
echo "  $output ($lines lines)"

echo "Reference data generated successfully."
