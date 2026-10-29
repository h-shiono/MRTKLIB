#!/bin/bash
# generate_reference_cma.sh — Generate CLAS CMA (QZNMA-CLAS) reference data
# using upstream claslib 0.9.0 (tag 090).  See issue #350 / epic #356.
#
# Usage: Run from MRTKLIB root directory:
#   bash tests/data/claslib/generate_reference_cma.sh
#
# Unlike generate_reference.sh, upstream is built from `git archive 090` in a
# temporary directory, so local edits in the upstream/claslib worktree cannot
# leak into the reference.
#
# Prerequisites:
#   - upstream/claslib clone containing tag 090
#   - macOS: -D_DARWIN_C_SOURCE exposes strtok_r under -std=c99
#   - LAPACK/BLAS (Accelerate on macOS), same backend as generate_reference.sh
#
# Output (tests/data/claslib/):
#   claslib_cma_testdata.tar.gz  — inputs (0627044J.bnx, 2023044J.l6d/.l6e,
#                                  pubkey/CMA_00{1,2}.der) + L6E dump CSVs
#                                  (cma_dump/parse_l6e_*.csv)
#   ref_L6_AUTH.nmea             — PPP-RTK solution, authentication on
#   ref_L6_AUTH_auth.nmea        — $PMSB,CMASF/CMASTS/CNTI authentication log
#   ref_L6_AUTH_noauth.nmea      — PPP-RTK solution, same inputs, no .l6e
#                                  (upstream forces clas_auth=0)
#
# Auxiliary files (grid, ATX, ERP, ISB, L2C shift) are unchanged in 090 and
# come from claslib_testdata.tar.gz.

set -euo pipefail

TAG=090
MRTKLIB_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CLASLIB_GIT="$MRTKLIB_ROOT/upstream/claslib"
OUTDIR="$MRTKLIB_ROOT/tests/data/claslib"
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

# require_nonempty <file...>: upstream tools can exit 0 without output.
require_nonempty() {
    local f
    for f in "$@"; do
        if [ ! -s "$f" ]; then
            echo "ERROR: expected output '$f' is missing or empty" >&2
            exit 1
        fi
    done
}

MACFLAGS="-Wall -O3 -DTRACE -DENAGAL -DENAQZS -DNFREQ=3 -DLAPACK -D_DARWIN_C_SOURCE -std=c99"
MACFLAGS="$MACFLAGS -I../../seclib/include -I../../src"

echo "=== Extracting claslib $TAG ($(git -C "$CLASLIB_GIT" rev-parse --short "$TAG")) ==="
git -C "$CLASLIB_GIT" archive "$TAG" | tar -x -C "$WORK"
DATADIR="$WORK/data"

echo "=== Building rnx2rtkp (ENA_CSSR_AUTH) ==="
cd "$WORK/util/rnx2rtkp"
run build_rnx2rtkp make CFLAGS="$MACFLAGS -DENA_PPP_RTK -DENA_REL_VRS -DENA_CSSR_AUTH"
sed $'s/\r$//' kinematic.conf | sed 's|\\|/|g' > kinematic_mac.conf

echo "=== Building ssr2osr ==="
cd "$WORK/util/ssr2osr"
run build_ssr2osr make CFLAGS="$MACFLAGS -DENA_SSR2OSR"

# test_L6_auth (util/rnx2rtkp/makefile).  Trace level 0: the trace file is not
# part of the reference.  Upstream prints wall-clock time in "first recieve"
# console messages; outputs themselves depend only on the inputs.
cd "$WORK/util/rnx2rtkp"
OPTT=(-ti 1 -ts 2023/02/13 09:00:00 -te 2023/02/13 09:59:59 -x 0)
OBS="$DATADIR/0627044J.bnx"
L6D="$DATADIR/2023044J.l6d"
L6E="$DATADIR/2023044J.l6e"

echo "=== Running test_L6_auth (authentication on) ==="
run rnx2rtkp_auth ./rnx2rtkp "${OPTT[@]}" -k kinematic_mac.conf "$OBS" "$L6D" "$L6E" -o auth.nmea
echo "=== Running baseline (no .l6e -> authentication off) ==="
run rnx2rtkp_noauth ./rnx2rtkp "${OPTT[@]}" -k kinematic_mac.conf "$OBS" "$L6D" -o noauth.nmea
require_nonempty auth.nmea auth_auth.nmea noauth.nmea

echo "=== Dumping L6E (ssr2osr -dump) ==="
mkdir -p "$WORK/pkg/cma_dump" "$WORK/pkg/pubkey"
(cd "$WORK/pkg/cma_dump" && run ssr2osr_dump "$WORK/util/ssr2osr/ssr2osr" -dump "$L6E")
require_nonempty "$WORK/pkg/cma_dump"/parse_l6e_{header,type13_1,type13_3,type13_11}.csv

# Write to tests/data only after every step has succeeded.
cp "$WORK/util/rnx2rtkp/auth.nmea" "$OUTDIR/ref_L6_AUTH.nmea"
cp "$WORK/util/rnx2rtkp/auth_auth.nmea" "$OUTDIR/ref_L6_AUTH_auth.nmea"
cp "$WORK/util/rnx2rtkp/noauth.nmea" "$OUTDIR/ref_L6_AUTH_noauth.nmea"

echo "=== Packaging claslib_cma_testdata.tar.gz ==="
cp "$OBS" "$L6D" "$L6E" "$WORK/pkg/"
cp "$DATADIR"/pubkey/CMA_*.der "$WORK/pkg/pubkey/"
# macOS bsdtar would otherwise add AppleDouble ._* members and
# com.apple.* xattr pax headers (GNU tar warns on extraction).
TAR_OPTS=()
if tar --version 2>/dev/null | grep -q bsdtar; then
    TAR_OPTS=(--no-xattrs --no-mac-metadata)
fi
COPYFILE_DISABLE=1 tar "${TAR_OPTS[@]}" -czf "$OUTDIR/claslib_cma_testdata.tar.gz" -C "$WORK/pkg" \
    0627044J.bnx 2023044J.l6d 2023044J.l6e pubkey cma_dump

echo ""
echo "=== Summary ==="
for f in ref_L6_AUTH.nmea ref_L6_AUTH_noauth.nmea; do
    awk -F, '/GGA/{q[$7]++; n++} END{printf "%-26s n=%d", FILENAME, n; for (k in q) printf " Q%s=%d", k, q[k]; print ""}' \
        "$OUTDIR/$f"
done
awk -F, '/CMASTS/{s[$13]++; n++} END{printf "CMASTS n=%d", n; for (k in s) printf " stat%s=%d", k, s[k]; print ""}' \
    "$OUTDIR/ref_L6_AUTH_auth.nmea"
ls -la "$OUTDIR"/claslib_cma_testdata.tar.gz "$OUTDIR"/ref_L6_AUTH*.nmea
