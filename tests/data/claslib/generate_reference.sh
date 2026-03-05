#!/bin/bash
# generate_reference.sh — Generate CLAS PPP-RTK reference data using upstream claslib
#
# Usage: Run from MRTKLIB root directory:
#   bash tests/data/claslib/generate_reference.sh
#
# Prerequisites:
#   - macOS: uses -D_DARWIN_C_SOURCE to expose strtok_r
#   - LAPACK/BLAS required (Accelerate framework on macOS)
#
# Output:
#   tests/data/claslib/claslib_testdata.tar.gz  — input data + ssr2osr reference
#   tests/data/claslib/ref_*.nmea               — rnx2rtkp reference output

set -euo pipefail

MRTKLIB_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CLASLIB="$MRTKLIB_ROOT/upstream/claslib"
DATADIR="$CLASLIB/data"
OUTDIR="$MRTKLIB_ROOT/tests/data/claslib"
TMPDIR_VRS="$(mktemp -d)"

RNX2RTKP_DIR="$CLASLIB/util/rnx2rtkp"
SSR2OBS_DIR="$CLASLIB/util/ssr2obs"

MACFLAGS="-Wall -O3 -DTRACE -DENAGAL -DENAQZS -DNFREQ=3 -DLAPACK -D_DARWIN_C_SOURCE"

trap "rm -rf $TMPDIR_VRS" EXIT

# ============================================================
# Step 1: Build rnx2rtkp
# ============================================================
echo "=== Building rnx2rtkp ==="
cd "$RNX2RTKP_DIR"
make clean 2>/dev/null || true
make CFLAGS="$MACFLAGS -I../../src -DENA_PPP_RTK -DENA_REL_VRS" 2>&1 | tail -3

# Create macOS-compatible conf files (backslash→slash, strip CR)
for f in kinematic.conf kinematic_vrs.conf kinematic_L1CL5.conf; do
    sed $'s/\r$//' "$f" | sed 's|\\|/|g' > "${f%.conf}_mac.conf"
done

# ============================================================
# Step 2: Build ssr2obs
# ============================================================
echo ""
echo "=== Building ssr2obs ==="
cd "$SSR2OBS_DIR"
make clean 2>/dev/null || true
make CFLAGS="$MACFLAGS -I../../src -DNEXOBS=2 -DDLL -DCSSR2OSR_VRS" 2>&1 | tail -3

sed $'s/\r$//' sample.conf | sed 's|\\|/|g' > sample_mac.conf

# ============================================================
# Step 3: Generate VRS observation files with ssr2obs (ssr2osr)
# ============================================================
echo ""
echo "=== Generating VRS observation data (ssr2osr reference) ==="
SSR2OBS="$SSR2OBS_DIR/ssr2obs"
SSR2OBS_CONF="$SSR2OBS_DIR/sample_mac.conf"

echo "  ssr2obs: VRS for 2019/239..."
"$SSR2OBS" -k "$SSR2OBS_CONF" \
    -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -ti 1 \
    "$DATADIR/sept_2019239.nav" "$DATADIR/2019239Q.l6" \
    -r -o "$TMPDIR_VRS/vrs2019239Q.obs" 2>/dev/null

echo "  ssr2obs: VRS for 2019/349 (ST12)..."
"$SSR2OBS" -k "$SSR2OBS_CONF" \
    -ts 2019/12/15 01:00:00 -te 2019/12/15 01:59:59 -ti 1 \
    "$DATADIR/sept_2019349.nav" "$DATADIR/2019349B.l6" \
    -r -o "$TMPDIR_VRS/vrs2019349B.obs" 2>/dev/null

echo "  ssr2obs: VRS CH1 for 2025/157..."
"$SSR2OBS" -k "$SSR2OBS_CONF" \
    -ts 2025/06/6 20:00:00 -te 2025/06/6 20:59:59 -ti 1 \
    "$DATADIR/0627157U.nav" "$DATADIR/2025157U_ch1.l6" \
    -r -o "$TMPDIR_VRS/2025157U_ch1.obs" 2>/dev/null

echo "  ssr2obs: VRS CH2 for 2025/157..."
"$SSR2OBS" -k "$SSR2OBS_CONF" \
    -ts 2025/06/6 20:00:00 -te 2025/06/6 20:59:59 -ti 1 \
    "$DATADIR/0627157U.nav" "$DATADIR/2025157U_ch2.l6" \
    -r -o "$TMPDIR_VRS/2025157U_ch2.obs" 2>/dev/null

# ============================================================
# Step 4: Create claslib_testdata.tar.gz
# ============================================================
echo ""
echo "=== Creating claslib_testdata.tar.gz ==="
STAGING="$(mktemp -d)"
trap "rm -rf $TMPDIR_VRS $STAGING" EXIT

# Input data files
for f in \
    0627239Q.obs 0627239Q.bnx 0627349AB.bnx 0627157U.bnx 0627022Q.bnx \
    sept_2019239.nav sept_2019349.nav 0627157U.nav \
    2019239Q.l6 2019349B.l6 2025157U_ch1.l6 2025157U_ch2.l6 2025022Q.l6 \
    clas_grid.def clas_grid.blq igu00p01.erp igs14_L5copy.atx isb.tbl l2csft.tbl; do
    cp "$DATADIR/$f" "$STAGING/"
done

# VRS reference data (ssr2osr output)
cp "$TMPDIR_VRS"/*.obs "$STAGING/"

cd "$STAGING"
tar czf "$OUTDIR/claslib_testdata.tar.gz" .
echo "  $(du -sh "$OUTDIR/claslib_testdata.tar.gz" | cut -f1) claslib_testdata.tar.gz"

# ============================================================
# Step 5: Run rnx2rtkp test cases (8 tests)
# ============================================================
echo ""
echo "=== Running rnx2rtkp reference tests ==="
RNX2RTKP="$RNX2RTKP_DIR/rnx2rtkp"
CONF_L6="$RNX2RTKP_DIR/kinematic_mac.conf"
CONF_VRS="$RNX2RTKP_DIR/kinematic_vrs_mac.conf"
CONF_L1CL5="$RNX2RTKP_DIR/kinematic_L1CL5_mac.conf"

# Use VRS files from staging (same as what's in tar.gz)
VRS_DIR="$STAGING"

echo "  test_L6: PPP-RTK with L6 (RINEX obs)..."
"$RNX2RTKP" -ti 1 -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -x 2 \
    -k "$CONF_L6" \
    "$DATADIR/0627239Q.obs" "$DATADIR/sept_2019239.nav" "$DATADIR/2019239Q.l6" \
    -o "$OUTDIR/ref_L6.nmea" 2>/dev/null

echo "  test_L6_bnx: PPP-RTK with L6 (BINEX obs)..."
"$RNX2RTKP" -ti 1 -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -x 2 \
    -k "$CONF_L6" \
    "$DATADIR/0627239Q.bnx" "$DATADIR/sept_2019239.nav" "$DATADIR/2019239Q.l6" \
    -o "$OUTDIR/ref_L6_bnx.nmea" 2>/dev/null

echo "  test_VRS: VRS-RTK..."
"$RNX2RTKP" -ti 1 -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -x 2 \
    -k "$CONF_VRS" \
    "$DATADIR/0627239Q.obs" "$DATADIR/sept_2019239.nav" "$VRS_DIR/vrs2019239Q.obs" \
    -o "$OUTDIR/ref_VRS.nmea" 2>/dev/null

echo "  test_ST12: PPP-RTK with SubType 12..."
"$RNX2RTKP" -ti 1 -ts 2019/12/15 01:00:00 -te 2019/12/15 01:59:59 -x 2 \
    -k "$CONF_L6" \
    "$DATADIR/0627349AB.bnx" "$DATADIR/sept_2019349.nav" "$DATADIR/2019349B.l6" \
    -o "$OUTDIR/ref_ST12.nmea" 2>/dev/null

echo "  test_ST12_VRS: VRS-RTK with SubType 12..."
"$RNX2RTKP" -ti 1 -ts 2019/12/15 01:00:00 -te 2019/12/15 01:59:59 -x 3 \
    -k "$CONF_VRS" \
    "$DATADIR/0627349AB.bnx" "$DATADIR/sept_2019349.nav" "$VRS_DIR/vrs2019349B.obs" \
    -o "$OUTDIR/ref_ST12_VRS.nmea" 2>/dev/null

echo "  test_L6_2CH: PPP-RTK with 2-channel L6..."
"$RNX2RTKP" -ti 1 -ts 2025/06/6 20:00:00 -te 2025/06/6 20:59:59 -x 2 \
    -k "$CONF_L6" \
    "$DATADIR/0627157U.bnx" "$DATADIR/2025157U_ch1.l6" "$DATADIR/2025157U_ch2.l6" \
    -o "$OUTDIR/ref_L6_2CH.nmea" -l6msg 1 2>/dev/null

echo "  test_L6_2CH_VRS: VRS-RTK with 2-channel L6..."
"$RNX2RTKP" -ti 1 -ts 2025/06/6 20:00:00 -te 2025/06/6 20:59:59 -x 1 \
    -k "$CONF_VRS" \
    "$DATADIR/0627157U.bnx" "$VRS_DIR/2025157U_ch1.obs" "$VRS_DIR/2025157U_ch2.obs" \
    -o "$OUTDIR/ref_L6_2CH_VRS.nmea" -l6msg 1 2>/dev/null

echo "  test_L6_L1CL5: PPP-RTK with L1C+L5..."
"$RNX2RTKP" -ti 1 -ts 2025/1/22 16:00:00 -te 2025/1/22 16:59:59 -x 1 \
    -k "$CONF_L1CL5" \
    "$DATADIR/0627022Q.bnx" "$DATADIR/2025022Q.l6" \
    -o "$OUTDIR/ref_L6_L1CL5.nmea" 2>/dev/null

# ============================================================
# Step 6: Cleanup build artifacts
# ============================================================
echo ""
echo "=== Cleanup ==="
cd "$RNX2RTKP_DIR" && make clean 2>/dev/null && rm -f kinematic_mac.conf kinematic_vrs_mac.conf kinematic_L1CL5_mac.conf
cd "$SSR2OBS_DIR"  && make clean 2>/dev/null && rm -f sample_mac.conf

# ============================================================
# Summary
# ============================================================
echo ""
echo "=== Summary ==="
echo ""
echo "Archive:"
echo "  $(du -sh "$OUTDIR/claslib_testdata.tar.gz")"
echo ""
echo "ssr2osr reference (VRS obs in tar.gz):"
for f in vrs2019239Q.obs vrs2019349B.obs 2025157U_ch1.obs 2025157U_ch2.obs; do
    lines=$(tar tzf "$OUTDIR/claslib_testdata.tar.gz" | grep -c "$f" || echo 0)
    echo "  $f: included=$lines"
done
echo ""
echo "rnx2rtkp reference:"
for f in "$OUTDIR"/ref_*.nmea; do
    total=$(grep -c 'GPGGA' "$f")
    fix=$(grep 'GPGGA' "$f" | awk -F',' '$7==4' | wc -l | tr -d ' ')
    float=$(grep 'GPGGA' "$f" | awk -F',' '$7==5' | wc -l | tr -d ' ')
    printf "  %-25s %5d epochs  Fix=%4d  Float=%d\n" "$(basename "$f"):" "$total" "$fix" "$float"
done
