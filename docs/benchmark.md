# Kinematic Positioning Benchmark (PPC-Dataset)

This benchmark evaluates MRTKLIB's kinematic (vehicle-mounted) positioning
performance using the open-data [PPC2024 (Precise Positioning Challenge)][ppc]
dataset from Chiba Institute of Technology.  It covers six urban driving runs
(Nagoya × 3, Tokyo × 3) and supports two positioning modes:

| Mode | Engine | Correction |
|------|--------|------------|
| CLAS | PPP-RTK | QZSS L6D (IS-QZSS-L6-003) |
| MADOCA | PPP | QZSS L6E (MADOCA-PPP) |

> **Note:** This benchmark is intentionally excluded from the regular CTest
> suite because it requires large external datasets.  Run it on demand.

---

## Dataset

### Overview

- **Vehicle receiver**: Septentrio mosaic-X5, 5 Hz, RINEX 3.04
- **Reference station**: Trimble NetR9/Alloy, 1 Hz, RINEX 3.04
- **Ground truth**: Applanix POS LV 220, provided as `reference.csv`

| Run | City | Date | Duration |
|-----|------|------|----------|
| nagoya/run1 | Nagoya | 2024-08-03 | ~26 min |
| nagoya/run2 | Nagoya | 2024-07-20 | ~32 min |
| nagoya/run3 | Nagoya | 2024-08-03 | ~17 min |
| tokyo/run1  | Tokyo  | 2024-07-23 | ~40 min |
| tokyo/run2  | Tokyo  | 2024-07-23 | ~30 min |
| tokyo/run3  | Tokyo  | 2024-07-23 | ~51 min |

### Manual Download

The PPC-Dataset (~200 MB) must be downloaded manually:

1. Visit the dataset page or use the direct SharePoint link provided by the
   PPC2024 organiser.
2. Download and unzip the archive.
3. Place the contents so that the layout matches:

```
data/benchmark/
  nagoya/
    run1/
      rover.obs
      base.nav
      base.obs
      reference.csv
      ...
    run2/ ...
    run3/ ...
  tokyo/
    run1/ ...
    ...
```

The `data/benchmark/` directory is listed in `.gitignore` and will not be
committed.

### reference.csv Format

```
GPS TOW (s), GPS Week, Latitude (deg), Longitude (deg), Ellipsoid Height (m),
ECEF X (m), ECEF Y (m), ECEF Z (m), Roll (deg), Pitch (deg), Heading (deg),
East Velocity (m/s), North Velocity (m/s), Up Velocity (m/s)
```

---

## Prerequisites

1. **Build MRTKLIB** (rnx2rtkp binary):

   ```bash
   cmake --preset default
   cmake --build build
   ```

2. **Extract test data** (ATX, ISB, ERP, etc. required by the CLAS conf):

   ```bash
   cd build && ctest -R fixture --output-on-failure
   ```

3. **Install Python dependencies**:

   ```bash
   cd scripts && python -m venv .venv && source .venv/bin/activate
   pip install numpy matplotlib
   ```

---

## Quick Start

### Step 1 — Download L6 correction data

```bash
cd scripts/benchmark
python download_l6.py --mode both
```

This downloads the CLAS L6D and MADOCA L6E files for all six runs into
`data/benchmark/l6/`.  Use `--dry-run` to preview the URLs without downloading.

Options:

| Option | Default | Description |
|--------|---------|-------------|
| `--l6-dir DIR` | `data/benchmark/l6` | Where to store L6 files |
| `--mode clas\|madoca\|both` | `both` | Which correction type |
| `--case ID[,ID...]` | all | Restrict to specific run IDs |
| `--dry-run` | off | Print URLs without downloading |

### Step 2 — Run the benchmark

```bash
cd scripts/benchmark
python run_benchmark.py --mode both
```

This calls `rnx2rtkp` for each run × mode combination, saves NMEA output to
`data/benchmark/results/`, and prints a summary table.

Results are cached: if the NMEA output file is newer than all its inputs,
the positioning step is skipped.  Use `--force` to re-run.

Full options:

| Option | Default | Description |
|--------|---------|-------------|
| `--dataset-dir DIR` | `data/benchmark` | PPC-Dataset root |
| `--l6-dir DIR` | `data/benchmark/l6` | L6 file cache |
| `--out-dir DIR` | `data/benchmark/results` | NMEA output dir |
| `--mode clas\|madoca\|both` | `both` | Positioning mode |
| `--case ID[,ID...]` | all | Run specific cases only |
| `--rnx2rtkp PATH` | auto | Path to rnx2rtkp binary |
| `--skip-download` | off | Skip L6 download |
| `--force` | off | Re-run even if cached |
| `--skip-epochs N` | 60 | Epochs excluded from metrics |
| `--plot` | off | Save ENU PNG per case |
| `-v / --verbose` | off | Show rnx2rtkp output |

### Step 3 — Compare a single result (optional)

```bash
cd scripts/benchmark
python compare_ppc.py \
    --ref ../../data/benchmark/nagoya/run1/reference.csv \
    ../../data/benchmark/results/nagoya_run1_clas.nmea
```

---

## v0.3.3 Benchmark Results

Results recorded on MRTKLIB v0.3.3, GNSS-only (no IMU), `--skip-epochs 60`.

**Rate% column:**
- CLAS / RTK: Q=4 integer fix rate
- MADOCA: fraction of epochs with 2D horizontal error < 30 cm (float PPP)

**TTFF column:**
- CLAS / RTK: first epoch of a ≥30-consecutive Q=4 run, relative to first matched epoch
- MADOCA: first epoch of a ≥30-consecutive sub-30 cm run

### Nagoya

| Case | Mode | N | Rate% | RMS 2D | 1σ (68%) | 95% | TTFF (s) |
|------|------|--:|------:|-------:|---------:|----:|---------:|
| nagoya_run1 | CLAS    | 7 525 | 17.1% | 42.78 m | 4.10 m | 53.21 m | 9 |
| nagoya_run1 | MADOCA  | 2 028 |  0.0% | 17.43 m | 2.11 m |  4.69 m | — |
| nagoya_run1 | RTK     | 7 158 |  7.6% | 11.14 m | 1.73 m |  5.19 m | 561 |
| nagoya_run2 | CLAS    | 9 390 | 30.3% | 305.61 m | 8.30 m | 73.90 m | 0 |
| nagoya_run2 | MADOCA  | 7 554 |  0.2% | 39.54 m | 2.96 m | 19.45 m | — |
| nagoya_run2 | RTK     | 9 251 | 10.8% |  4.54 m | 3.65 m |  8.92 m | 352 |
| nagoya_run3 | CLAS    | 5 141 |  7.0% | 12.83 m | 6.63 m | 28.79 m | 9 |
| nagoya_run3 | MADOCA  | 2 813 |  2.1% |  2.65 m | 2.25 m |  4.29 m | — |
| nagoya_run3 | RTK     | 5 087 | 20.1% |  5.49 m | 4.68 m | 11.60 m | 379 |

### Tokyo

| Case | Mode | N | Rate% | RMS 2D | 1σ (68%) | 95% | TTFF (s) |
|------|------|--:|------:|-------:|---------:|----:|---------:|
| tokyo_run1 | CLAS    | 11 867 |  5.2% | 627.06 m |  7.91 m | 277.34 m | 15 |
| tokyo_run1 | MADOCA  |  3 144 | 16.6% |   1.83 m |  0.98 m |   1.96 m | 0 |
| tokyo_run1 | RTK     | 11 437 | 16.4% |  18.12 m |  9.64 m |  40.55 m | 693 |
| tokyo_run2 | CLAS    |  9 091 | 21.7% |  33.43 m |  2.39 m |  33.47 m | 368 |
| tokyo_run2 | MADOCA  |  8 219 |  6.7% |   2.89 m |  1.18 m |   4.58 m | 464 |
| tokyo_run2 | RTK     |  8 625 |  5.7% |  34.14 m | 16.66 m |  70.55 m | 926 |
| tokyo_run3 | CLAS    | 15 241 |  7.4% |  41.46 m |  3.74 m |  29.61 m | 28 |
| tokyo_run3 | MADOCA  |  2 855 |  3.4% |   0.72 m |  0.75 m |   1.06 m | 26 |
| tokyo_run3 | RTK     | 14 757 |  2.3% |  24.59 m |  5.97 m |  69.71 m | 1331 |

### Notes

- **RMS 2D (all)** includes all solution quality levels (fix + float + SPP).
  Large values for CLAS/RTK reflect float/SPP epochs with errors of tens to
  hundreds of metres, which dominate in urban canyons.
- **MADOCA low N** in some runs (e.g. nagoya_run1: 2 028 vs 7 525 for CLAS)
  reflects **filter divergence**: MADOCA PPP outputs Q=0 (no solution) for
  epochs where multipath or satellite geometry causes the float filter to lose
  lock.  Q=0 GGA sentences have empty lat/lon fields and are excluded from
  comparison.  In nagoya_run1, only 27% of rover epochs have a valid MADOCA
  solution; in nagoya_run2 and tokyo_run2 (more open sky), coverage is ~100%.
  This is a real performance limitation — not a software or configuration issue.
- **CLAS nagoya_run2 TTFF=0** means the very first epoch after the skip window
  was already in a sustained fix run.
- RTK baselines range from ~13 km (Nagoya) to ~27 km (Tokyo), which is long for
  kinematic RTK; float solutions dominate accordingly.

---

## Metrics Definitions

| Metric | Description |
|--------|-------------|
| **N** | Number of matched epochs (reference ↔ NMEA within 0.15 s) |
| **Fix%** | Percentage of epochs with GGA quality = 4 (integer AR fix) |
| **RMS_2D (all)** | Horizontal RMS error across all matched epochs |
| **RMS_3D (all)** | 3D RMS error across all matched epochs |
| **RMS_2D (fix)** | Horizontal RMS for Q=4 epochs only; `nan` if no fix |
| **Conv_s** | Seconds from first matched epoch to first run of ≥30 consecutive Q=4 |

ENU errors are computed per-epoch using the corresponding ground-truth
coordinate as the reference origin (moving-base projection).

---

## Configuration

The benchmark uses dedicated configuration files:

- `conf/benchmark/clas.conf` — CLAS PPP-RTK
- `conf/benchmark/madoca.conf` — MADOCA PPP

Key differences from the standard test configurations:

| Parameter | Value | Reason |
|-----------|-------|--------|
| `ant1-anttype` | `*` | rover RINEX has "Unknown" antenna |
| `pos2-isb` | `off` | no ISB calibration for Septentrio mosaic-X5 |
| `pos1-dynamics` | `on` | kinematic mode (MADOCA) |
| `out-solformat` | `nmea` | GGA parsed by `compare_ppc.py` |

---

## Known Limitations

- **Urban multipath**: The Nagoya and Tokyo datasets include heavy shadowing and
  multipath.  Fix rates and 3D accuracy will be lower than open-sky tests.
- **No antenna calibration**: rover RINEX antenna field is "Unknown", so no PCV
  correction is applied.  This may affect Up accuracy by a few centimetres.
- **No ISB correction**: Septentrio mosaic-X5 is not in the ISB table.
  QZSS inter-system biases will be slightly mis-corrected.
- **MADOCA float only**: MADOCA-PPP mode does not produce integer AR fixes
  (Q=4) — Fix% and RMS_2D(fix) will be `nan`.
- **IMU not used**: Only GNSS-only positioning is evaluated.

---

## Acknowledgements

The PPC-Dataset is kindly provided as open data by:

> **Taro Suzuki**, Chiba Institute of Technology
> *PPC-Dataset — GNSS/IMU driving data for precise positioning research*
> <https://github.com/taroz/PPC-Dataset>

We gratefully acknowledge Prof. Suzuki for making this dataset publicly
available.  Please cite the PPC2024 materials when publishing results derived
from this dataset.

---

## References

- [PPC2024 overview (Japanese)][ppc-pdf]
- [PPC2024 results (Japanese)][ppc-res]
- [PPC-Dataset GitHub][ppc]

[ppc]: https://github.com/taroz/PPC-Dataset
[ppc-pdf]: http://taroz.net/data/PPC2024.pdf
[ppc-res]: http://taroz.net/data/PPC2024_results.pdf
