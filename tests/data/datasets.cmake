# tests/data/datasets.cmake : test datasets hosted as release assets (#386)
#
# One line per dataset version: release tag / asset <name-vN>(.tar.gz) of
# h-shiono/mrtklib-data and its SHA256. A published asset is never replaced;
# a changed dataset is a new <name-vN+1> line. Build assets with
# scripts/tests/make_dataset_asset.py (reproducible, prints the line below).
#
# Provenance of each dataset is documented next to its consumers.

# Galileo HAS: G5P3162a_15m.sbf (mosaic-G5, 2026-06-10, 15 min); see the HAS
# section of CMakeLists.txt and docs/design/gal-has.md §11.
mrtk_dataset(has-v1 SHA256 15d2723e6c9d0cea4ca26ec9af2d3a4776b62dda1efadd1b52b4a1cdbd2d5022)

# QZSS L6 handover: five 15-min windows (NEO-D9C UBX + mosaic-G5 SBF, mosaic-CLAS
# SBF), DOY 2026-286; see tests/data/handover/README.md (#372).
mrtk_dataset(handover-v1 SHA256 2690ac610a3696731dcfa629d2f98a9dfd54fef9126ae9b6de5c3349a3792fe9)

# MADOCA-PPP IS-QZSS-MDC-005: MADOCALIB 2.2 sample, MIZU 2026/03/28 Session A
# (OBS cut, BRDC NAV, L6E PRN 207, MIZU antenna from igs20_2290.atx); see
# tests/data/madocalib_mdc005/generate_reference_mdc005.sh (#401).
mrtk_dataset(madocalib_mdc005-v1 SHA256 c2179bda50c86608403a44bc4289befa2bbbcfae2558dcea0c72c179a58ec00c)

# CLAS L6 frame gaps: G5P3296u_20m.sbf (live mosaic-G5, 2026-10-23, 20 min, SBF
# blocks dropped by the receiver); see tests/data/clas_l6gap/README.md (#363).
mrtk_dataset(clas_l6gap-v1 SHA256 365fe0466a64badb3f52ccebe85ace855615d08e8908848ba1999323cad7bc1e)

# mosaic-G5 open-service SSR hour, kubolab 2026-10-23 19:59:30-21:00 GPST:
# BeiDou PPP-B2b (GEO BDSRawB2b + BDSCNav1) and SouthPAN PVS (GEORawL5 PRN 122),
# plus a cut RINEX 4 broadcast navigation file; see tests/data/g5p3_296u/README.md
# (#417, #418).
mrtk_dataset(g5p3_296u-v1 SHA256 923b2fe8e276664b8ad809947eaca2afc5577a9eca39cb7274b50781b5d44106)

# SouthPAN PVS in the service area: ALBY 2025-233 h (SIS, Kamakura log) and
# ALIC 2025-110 f (DAS log) from cssrlib-data, with cut DLR broadcast
# navigation; see tests/data/southpan/README.md (#418).
mrtk_dataset(southpan-v1 SHA256 cf55f6ce2ffb900606078cd2366d0ccf4a843fb7707c8709314befb3e73ddefe)
