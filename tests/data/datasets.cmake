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
