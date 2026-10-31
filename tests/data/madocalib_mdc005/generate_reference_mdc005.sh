#!/bin/bash
set -euo pipefail

# Generate the MADOCA-PPP IS-QZSS-MDC-005 test dataset and its references (#401)
#
#   Phase 1 — Build the dataset asset madocalib_mdc005-v1 (--prepare)
#     From upstream/madocalib (MADOCALIB 2.2) sample_data: MIZU OBS cut to
#     Session A (text cut, records verbatim), full-day BRDC NAV, L6E PRN 207 hour A,
#     and the MIZU receiver antenna (SEPCHOKE_B3E6 NONE) extracted from
#     igs20_2290.atx. Packed with scripts/tests/make_dataset_asset.py, which
#     prints the tests/data/datasets.cmake line and the publish commands.
#
#   Phase 2 — Build madocalib rnx2rtkp as upstream ships it: internal LU, no
#     -DLAPACK (see tests/data/madocalib/generate_reference_madocalib.sh, #302).
#
#   Phase 3 — Run it on the dataset with upstream's own 2.2 sample configs
#     (upstream_conf/, only file-rcvantfile adjusted):
#       1. PPP     (sample.conf,       navsys 29: no BeiDou)
#       2. PPP-AR  (sample_pppar.conf, navsys 61: BeiDou, pos2-bds2prn C09,C10,
#                   pos2-bds3igso C06-C08)
#
# Data: MIZU, 2026/03/28 Session A (00:00:00-00:59:30 GPST). The *_BDSTEST
#       RINEX files are upstream's BDS PRN-reassignment test data.
#
# Usage:
#   bash tests/data/madocalib_mdc005/generate_reference_mdc005.sh --prepare OUTDIR
#   bash tests/data/madocalib_mdc005/generate_reference_mdc005.sh
#       (Phase 2-3; the dataset must be extracted in tests/data/madocalib_mdc005,
#        e.g. by `ctest -R madocalib_mdc005_setup`)

PREPARE_DIR=""
if [[ "${1:-}" == "--prepare" ]]; then
    PREPARE_DIR="${2:?--prepare needs an output directory}"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$ROOT_DIR"

DATADIR=tests/data/madocalib_mdc005
UPSTREAM=upstream/madocalib
UPSTREAM_DATA="${UPSTREAM}/sample_data/data"

if ! grep -q '#define VER_MADOCALIB "2.2"' "${UPSTREAM}/src/rtklib.h"; then
    echo "ERROR: ${UPSTREAM} is not MADOCALIB 2.2 (git -C ${UPSTREAM} checkout ver.2.2)" >&2
    exit 1
fi

OBS="${DATADIR}/MIZU00JPN_R_20260870000_01H_30S_MO_BDSTEST.rnx"
NAV="${DATADIR}/BRDC00IGS_R_20260870000_01D_MN_BDSTEST.rnx"
L6E="${DATADIR}/2026087A.207.l6"
ATX="${DATADIR}/igs20_2290_MIZU.atx"

# ── Phase 1: dataset asset ────────────────────────────────────────────────────
if [[ -n "$PREPARE_DIR" ]]; then
    mkdir -p "$PREPARE_DIR"
    work="$(mktemp -d)"

    # Cut by text, header and epoch records verbatim: `mrtk convert` re-encodes
    # into NFREQ+NEXOBS slots and drops signals (R3Q, E6B, J1Z, J5P here).
    awk '
        !hdr                             { print; if (/END OF HEADER/) hdr = 1; next }
        /^>/                             { keep = ($5 == 0) }
        keep                             { print }
    ' "${UPSTREAM_DATA}/rinex/obs/2026/087/MIZU00JPN_R_20260870000_01D_30S_MO_BDSTEST.rnx" \
        > "${work}/$(basename "$OBS")"
    echo "  OBS cut: $(grep -c '^>' "${work}/$(basename "$OBS")") epochs"

    cp "${UPSTREAM_DATA}/rinex/BRDC/2026/087/BRDC00IGS_R_20260870000_01D_MN_BDSTEST.rnx" "${work}/"
    cp "${UPSTREAM_DATA}/l6_is-qzss-mdc-005/2026/087/2026087A.207.l6" "${work}/"

    # ATX header + the one receiver antenna block MIZU uses
    awk '
        /END OF HEADER/          { print; hdr = 1; next }
        !hdr                     { print; next }
        /START OF ANTENNA/       { blk = $0 "\n"; inblk = 1; keep = 0; next }
        inblk                    { blk = blk $0 "\n" }
        inblk && /TYPE \/ SERIAL NO/ && substr($0, 1, 20) == "SEPCHOKE_B3E6   NONE" { keep = 1 }
        /END OF ANTENNA/         { if (keep) printf "%s", blk; inblk = 0 }
    ' "${UPSTREAM_DATA}/igs20_2290.atx" > "${work}/$(basename "$ATX")"
    grep -q "SEPCHOKE_B3E6   NONE" "${work}/$(basename "$ATX")" || { echo "ERROR: antenna not found" >&2; exit 1; }

    python3 scripts/tests/make_dataset_asset.py madocalib_mdc005-v1 "$PREPARE_DIR" \
        "${work}/$(basename "$OBS")" "${work}/$(basename "$NAV")" \
        "${work}/$(basename "$L6E")" "${work}/$(basename "$ATX")"
    rm -rf "$work"
    exit 0
fi

# ── Phase 2: build madocalib rnx2rtkp ─────────────────────────────────────────
for f in "$OBS" "$NAV" "$L6E" "$ATX"; do
    [[ -f "$f" ]] || { echo "ERROR: $f missing (extract madocalib_mdc005-v1 first)" >&2; exit 1; }
done

if [[ "$(uname)" == "Darwin" ]]; then LDLIBS="-lm"; else LDLIBS="-lm -lrt"; fi
BUILDDIR="${ROOT_DIR}/build/madocalib_mdc005_ref"
mkdir -p "$BUILDDIR"
SRC="${UPSTREAM}/src"
CFLAGS="-Wall -O3 -ansi -pedantic -I${SRC} -DTRACE -DENAGLO -DENAQZS -DENAGAL -DENACMP -DENAIRN -DNFREQ=5 -DNEXOBS=5 -g"
SRCS=("${UPSTREAM}/app/consapp/rnx2rtkp/rnx2rtkp.c")
for s in rtkcmn rinex rtkpos postpos solution lambda geoid sbas preceph pntpos ephemeris options \
         ppp ppp_ar ppp_iono rtcm rtcm2 rtcm3 rtcm3e ionex tides mdccssr mdciono; do
    SRCS+=("${SRC}/${s}.c")
done
# shellcheck disable=SC2086
cc $CFLAGS -o "${BUILDDIR}/rnx2rtkp" "${SRCS[@]}" $LDLIBS

# ── Phase 3: references ───────────────────────────────────────────────────────
run() {
    local conf="$1" out="$2"
    "${BUILDDIR}/rnx2rtkp" -k "$conf" -ts 2026/03/28 0:00:00 -te 2026/03/28 0:59:30 -ti 30 \
        -ant "$ATX" -o "$out" "$OBS" "$NAV" "$L6E"
    [[ -s "$out" ]] || { echo "ERROR: $out empty" >&2; exit 1; }
    echo "  $out ($(grep -vc '^%' "$out") epochs)"
}
run "${DATADIR}/upstream_conf/sample.conf"       "${DATADIR}/madocalib_MIZU_20260328-0000.pp.pos"
run "${DATADIR}/upstream_conf/sample_pppar.conf" "${DATADIR}/madocalib_MIZU_20260328-0000.pppar.pos"

{
    echo "# Provenance of the madocalib_MIZU_20260328-0000.*.pos references."
    echo "# Written by generate_reference_mdc005.sh; do not edit by hand."
    echo "generated : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "upstream  : VER_MADOCALIB=2.2 commit $(git -C "$UPSTREAM" rev-parse HEAD)"
    echo "backend   : internal LU (no -DLAPACK)"
    echo "cflags    : $CFLAGS"
    echo "compiler  : $(cc --version | head -1)"
    echo "platform  : $(uname -srm)"
} > "${DATADIR}/reference_provenance.txt"
