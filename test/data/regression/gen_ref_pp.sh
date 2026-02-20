#!/bin/bash
set -euo pipefail

# Generate Reference Position for PPP and SPP using rnx2rtkp
#
# Usage:
#   bash test/data/regression/gen_ref_pp.sh            # without trace
#   bash test/data/regression/gen_ref_pp.sh --trace     # with trace (level 5)
#   bash test/data/regression/gen_ref_pp.sh --trace 3   # with trace (custom level)

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

# Temporary files to clean up (populated as script runs)
CLEANUP_FILES=()

cleanup() {
    echo "Cleaning up..."
    for f in ${CLEANUP_FILES[@]+"${CLEANUP_FILES[@]}"}; do
        rm -f "$f"
    done
    # Files extracted by tar
    rm -f data/2024235L.209.l6
    rm -f data/MALIB_OSS_data_obsnav_240822-1100.*
    rm -f data/MALIB_OSS_data_l6e_240822-1100.*
    rm -f data/igs14*.atx
}
trap cleanup EXIT

# Build binary
echo "Building rnx2rtkp..."
make -C app/consapp/rnx2rtkp/gcc clean
make -C app/consapp/rnx2rtkp/gcc
mv app/consapp/rnx2rtkp/gcc/rnx2rtkp .
CLEANUP_FILES+=(./rnx2rtkp)

# Extract test data
echo "Extracting data..."
tar -xzf data/MALIB_OSS_data.tar.gz

# Copy config file
cp bin/rnx2rtkp.conf .
CLEANUP_FILES+=(./rnx2rtkp.conf)

# Input files
obs=data/MALIB_OSS_data_obsnav_240822-1100.obs
nav=data/MALIB_OSS_data_obsnav_240822-1100.nav
l6e=data/2024235L.209.l6

# Output directory
output_dir=test/data/regression
mkdir -p "$output_dir"

# Execute rnx2rtkp (PPP)
output="${output_dir}/MALIB_OSS_data_obsnav_240822-1100.pp.pos"
echo "Running rnx2rtkp for PPP..."
./rnx2rtkp -k rnx2rtkp.conf ${TRACE_OPTS[@]+"${TRACE_OPTS[@]}"} -o "$output" "$obs" "$nav" "$l6e"

# Execute rnx2rtkp (SPP)
output_spp="${output_dir}/MALIB_OSS_data_obsnav_240822-1100.pp.spp.pos"
echo "Running rnx2rtkp for SPP..."
./rnx2rtkp -p 0 -sys G,R,E,J ${TRACE_OPTS[@]+"${TRACE_OPTS[@]}"} -o "$output_spp" "$obs" "$nav"

# Verify outputs
echo "Verifying outputs..."
for f in "$output" "$output_spp"; do
    if [[ ! -s "$f" ]]; then
        echo "ERROR: Output file not generated or empty: $f" >&2
        exit 1
    fi
    lines=$(wc -l < "$f")
    echo "  $f ($lines lines)"
done

echo "Reference data generated successfully."
