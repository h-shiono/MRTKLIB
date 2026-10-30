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
