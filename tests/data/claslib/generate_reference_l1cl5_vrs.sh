#!/bin/bash
# generate_reference_l1cl5_vrs.sh — QZS L1C + L5 VRS-RTK reference (#349)
# using upstream claslib 0.9.1 (tag 091).
#
# Usage: Run from MRTKLIB root directory:
#   bash tests/data/claslib/generate_reference_l1cl5_vrs.sh [OBS_OUT]
#
# Upstream is built from `git archive 091` in a temporary directory (as
# generate_reference_cma.sh), so local edits in the upstream/claslib worktree
# cannot leak into the reference.  Upstream ships neither the VRS
# observations nor a reference solution for this case: both are generated
# here (util/ssr2obs `test1r_L1CL5`, util/rnx2rtkp `test_L1CL5_VRS`).
#
# Prerequisites: as generate_reference_cma.sh (upstream/claslib clone with
# tag 091, LAPACK/BLAS; macOS needs -D_DARWIN_C_SOURCE under -std=c99).
#
# Output:
#   tests/data/claslib/ref_L6_L1CL5_VRS.nmea  — rnx2rtkp kinematic_L1CL5_vrs
#   tests/data/claslib/brdc_0627022Q.nav      — upstream data/0627022Q.nav
#   OBS_OUT (optional)                        — the upstream VRS observations
#       (2025022Q_L1CL5.obs, 5.9 MB: not committed; the regression test runs
#       on the output of `mrtk ssr2obs`)

set -euo pipefail

TAG=091
MRTKLIB_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CLASLIB_GIT="$MRTKLIB_ROOT/upstream/claslib"
OUTDIR="$MRTKLIB_ROOT/tests/data/claslib"
OBS_OUT="${1:-}"
# the steps below change directory: keep a relative OBS_OUT at the caller's
if [ -n "$OBS_OUT" ] && [ "${OBS_OUT#/}" = "$OBS_OUT" ]; then
    OBS_OUT="$PWD/$OBS_OUT"
fi
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
LOGDIR="$WORK/logs"
mkdir -p "$LOGDIR"

# run <label> <cmd...>: log stdout/stderr; on failure show the log tail before
# the EXIT trap removes $WORK.
run() {
    local label="$1"; shift
    if ! "$@" >"$LOGDIR/$label.log" 2>&1; then
        echo "ERROR: step '$label' failed (exit $?). Last 40 lines of log:" >&2
        tail -n 40 "$LOGDIR/$label.log" >&2
        exit 1
    fi
}

# to_mac <conf>: Windows path separators and CRLF of the upstream confs
to_mac() {
    sed $'s/\r$//' "$1" | sed 's|\\|/|g' > "${1%.conf}_mac.conf"
}

MACFLAGS="-Wall -O3 -DTRACE -DENAGAL -DENAQZS -DNFREQ=3 -DLAPACK -D_DARWIN_C_SOURCE -std=c99"
MACFLAGS="$MACFLAGS -I../../seclib/include -I../../src"

echo "=== Extracting claslib $TAG ($(git -C "$CLASLIB_GIT" rev-parse --short "$TAG")) ==="
git -C "$CLASLIB_GIT" archive "$TAG" | tar -x -C "$WORK"
DATADIR="$WORK/data"
OPTT=(-ts 2025/1/22 16:00:00 -te 2025/1/22 16:59:59 -ti 1)

echo "=== Building rnx2rtkp ==="
cd "$WORK/util/rnx2rtkp"
run build_rnx2rtkp make CFLAGS="$MACFLAGS -DENA_PPP_RTK -DENA_REL_VRS -DENA_CSSR_AUTH"
to_mac kinematic_L1CL5_vrs.conf

echo "=== Building ssr2obs ==="
cd "$WORK/util/ssr2obs"
run build_ssr2obs make CFLAGS="$MACFLAGS -DNEXOBS=2 -DDLL -DCSSR2OSR_VRS"
to_mac sample_L1CL5.conf

echo "=== ssr2obs: VRS observations (test1r_L1CL5) ==="
run ssr2obs ./ssr2obs -k sample_L1CL5_mac.conf "${OPTT[@]}" \
    "$DATADIR/0627022Q.nav" "$DATADIR/2025022Q.l6" -r -o "$DATADIR/2025022Q_L1CL5.obs"

echo "=== rnx2rtkp: VRS-RTK (test_L1CL5_VRS) ==="
cd "$WORK/util/rnx2rtkp"
run rnx2rtkp ./rnx2rtkp "${OPTT[@]}" -x 0 -k kinematic_L1CL5_vrs_mac.conf \
    "$DATADIR/0627022Q.bnx" "$DATADIR/2025022Q_L1CL5.obs" -o l1cl5_vrs.nmea

for f in "$DATADIR/2025022Q_L1CL5.obs" l1cl5_vrs.nmea; do
    if [ ! -s "$f" ]; then
        echo "ERROR: expected output '$f' is missing or empty" >&2
        exit 1
    fi
done

# upstream writes CRLF; the repository keeps the references with LF
tr -d '\r' < l1cl5_vrs.nmea > "$OUTDIR/ref_L6_L1CL5_VRS.nmea"
cp "$DATADIR/0627022Q.nav" "$OUTDIR/brdc_0627022Q.nav"
if [ -n "$OBS_OUT" ]; then
    cp "$DATADIR/2025022Q_L1CL5.obs" "$OBS_OUT"
fi

echo ""
echo "=== Summary ==="
awk -F, '/GGA/{q[$7]++; n++} END{printf "ref_L6_L1CL5_VRS.nmea n=%d", n; for (k in q) printf " Q%s=%d", k, q[k]; print ""}' \
    "$OUTDIR/ref_L6_L1CL5_VRS.nmea"
