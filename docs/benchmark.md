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

### Solution Quality Tiers

Each case/mode is broken down into three tiers:

| Tier | GGA quality flags | Description |
|------|-------------------|-------------|
| **FIX** | Q=4 | Integer ambiguity fix (CLAS / RTK) |
| **FF** | Q=3, 4, 5 | Fix + float + PPP float; excludes SPP (Q=1) |
| **ALL** | any | Every matched epoch including SPP fallback |

**Rate% column:**
- FIX row: Q=4 fix rate (CLAS / RTK); always 0% for MADOCA
- FF row: fraction of non-SPP epochs
- ALL row (MADOCA only): fraction of epochs with 2D error < 30 cm

**TTFF column:**
- CLAS / RTK: first epoch of a ≥30-consecutive Q=4 run, relative to first matched epoch (shown on FIX row)
- MADOCA: first epoch of a ≥30-consecutive sub-30 cm run (shown on ALL row)

### Nagoya

| Case | Mode | Tier | N | Rate% | RMS 2D | 1σ (68%) | 95% | TTFF (s) |
|------|------|------|--:|------:|-------:|---------:|----:|---------:|
| nagoya_run1 | CLAS   | FIX |  1 285 | 17.1% |   1.24 m | 0.40 m |  0.45 m |   9 |
|             |        | FF  |  1 831 | 24.3% |   2.86 m | 0.44 m |  7.84 m |   — |
|             |        | ALL |  7 525 |     — |  42.78 m | 4.10 m | 53.21 m |   — |
| nagoya_run1 | MADOCA | FIX |      0 |  0.0% |      nan |    nan |     nan |   — |
|             |        | FF  |  1 968 |100.0% |  17.43 m | 2.11 m |  4.69 m |   — |
|             |        | ALL |  1 968 |  0.0% |  17.43 m | 2.11 m |  4.69 m |   — |
| nagoya_run1 | RTK    | FIX |    543 |  7.6% |  11.09 m | 2.24 m |  8.09 m | 561 |
|             |        | FF  |  7 158 |100.0% |  11.14 m | 1.73 m |  5.19 m |   — |
|             |        | ALL |  7 158 |     — |  11.14 m | 1.73 m |  5.19 m |   — |
| nagoya_run2 | CLAS   | FIX |  2 849 | 30.3% |   1.17 m | 0.72 m |  0.98 m |   0 |
|             |        | FF  |  6 002 | 63.9% |  15.90 m | 1.21 m | 36.59 m |   — |
|             |        | ALL |  9 390 |     — | 305.61 m | 8.30 m | 73.90 m |   — |
| nagoya_run2 | MADOCA | FIX |      0 |  0.0% |      nan |    nan |     nan |   — |
|             |        | FF  |  7 494 |100.0% |  39.54 m | 2.96 m | 19.45 m |   — |
|             |        | ALL |  7 494 |  0.2% |  39.54 m | 2.96 m | 19.45 m |   — |
| nagoya_run2 | RTK    | FIX |    999 | 10.8% |   6.76 m | 5.07 m |  8.93 m | 352 |
|             |        | FF  |  9 251 |100.0% |   4.54 m | 3.65 m |  8.92 m |   — |
|             |        | ALL |  9 251 |     — |   4.54 m | 3.65 m |  8.92 m |   — |
| nagoya_run3 | CLAS   | FIX |    360 |  7.0% |   0.45 m | 0.35 m |  1.06 m |   9 |
|             |        | FF  |  1 970 | 38.3% |  12.15 m | 2.63 m | 38.24 m |   — |
|             |        | ALL |  5 141 |     — |  12.83 m | 6.63 m | 28.79 m |   — |
| nagoya_run3 | MADOCA | FIX |      0 |  0.0% |      nan |    nan |     nan |   — |
|             |        | FF  |  2 753 |100.0% |   2.65 m | 2.25 m |  4.29 m |   — |
|             |        | ALL |  2 753 |  2.1% |   2.65 m | 2.25 m |  4.29 m |   — |
| nagoya_run3 | RTK    | FIX |  1 020 | 20.1% |   4.44 m | 3.81 m |  8.13 m | 379 |
|             |        | FF  |  5 087 |100.0% |   5.49 m | 4.68 m | 11.60 m |   — |
|             |        | ALL |  5 087 |     — |   5.49 m | 4.68 m | 11.60 m |   — |

### Tokyo

| Case | Mode | Tier | N | Rate% | RMS 2D | 1σ (68%) | 95% | TTFF (s) |
|------|------|------|--:|------:|-------:|---------:|----:|---------:|
| tokyo_run1 | CLAS   | FIX |    612 |  5.2% |   0.87 m |  0.24 m |  2.48 m |  15 |
|            |        | FF  |  9 235 | 77.8% | 119.24 m |  3.46 m | 25.55 m |   — |
|            |        | ALL | 11 867 |     — | 627.06 m |  7.91 m |277.34 m |   — |
| tokyo_run1 | MADOCA | FIX |      0 |  0.0% |      nan |     nan |     nan |   — |
|            |        | FF  |  3 084 |100.0% |   1.83 m |  0.98 m |  1.96 m |   — |
|            |        | ALL |  3 084 | 16.6% |   1.83 m |  0.98 m |  1.96 m |   0 |
| tokyo_run1 | RTK    | FIX |  1 870 | 16.4% |   9.47 m |  6.35 m | 12.49 m | 693 |
|            |        | FF  | 11 437 |100.0% |  18.12 m |  9.64 m | 40.55 m |   — |
|            |        | ALL | 11 437 |     — |  18.12 m |  9.64 m | 40.55 m |   — |
| tokyo_run2 | CLAS   | FIX |  1 972 | 21.7% |   0.59 m |  0.12 m |  1.04 m | 368 |
|            |        | FF  |  6 841 | 75.3% |  22.99 m |  1.19 m | 14.92 m |   — |
|            |        | ALL |  9 091 |     — |  33.43 m |  2.39 m | 33.47 m |   — |
| tokyo_run2 | MADOCA | FIX |      0 |  0.0% |      nan |     nan |     nan |   — |
|            |        | FF  |  8 159 |100.0% |   2.89 m |  1.18 m |  4.58 m |   — |
|            |        | ALL |  8 159 |  6.7% |   2.89 m |  1.18 m |  4.58 m | 464 |
| tokyo_run2 | RTK    | FIX |    494 |  5.7% |  28.45 m |  9.83 m | 62.92 m | 926 |
|            |        | FF  |  8 625 |100.0% |  34.14 m | 16.66 m | 70.55 m |   — |
|            |        | ALL |  8 625 |     — |  34.14 m | 16.66 m | 70.55 m |   — |
| tokyo_run3 | CLAS   | FIX |  1 129 |  7.4% |   0.80 m |  0.08 m |  1.83 m |  28 |
|            |        | FF  |  2 629 | 17.2% |   3.76 m |  0.63 m |  2.43 m |   — |
|            |        | ALL | 15 241 |     — |  41.46 m |  3.74 m | 29.61 m |   — |
| tokyo_run3 | MADOCA | FIX |      0 |  0.0% |      nan |     nan |     nan |   — |
|            |        | FF  |  2 795 |100.0% |   0.72 m |  0.75 m |  1.06 m |   — |
|            |        | ALL |  2 795 |  3.4% |   0.72 m |  0.75 m |  1.06 m |  26 |
| tokyo_run3 | RTK    | FIX |    341 |  2.3% |  26.57 m |  2.73 m | 88.33 m |1331 |
|            |        | FF  | 14 757 |100.0% |  24.59 m |  5.97 m | 69.71 m |   — |
|            |        | ALL | 14 757 |     — |  24.59 m |  5.97 m | 69.71 m |   — |

### Notes

- **CLAS FIX accuracy** (Q=4 only): RMS 2D is **0.4–1.2 m** across all runs —
  consistent PPP-RTK performance under urban multipath.
- **CLAS FF vs ALL gap**: the large ALL RMS (12–627 m) reflects SPP fallback
  epochs (Q=1) that dominate in dense canyons.  FF tier (24–78%) excludes these.
- **MADOCA FF = ALL**: MADOCA outputs only Q=3 (PPP float) or Q=0 (no solution);
  Q=0 epochs are already filtered, so FF ≡ ALL for MADOCA.
- **RTK FF = ALL**: RTK output is Q=4 (fix) or Q=5 (float) only — no SPP
  fallback — so FF always equals ALL.
- **RTK FIX quality**: large FIX RMS in some runs (e.g. tokyo_run2: 28.5 m)
  indicates wrongly-fixed integer solutions at 13–27 km baselines.  Long-baseline
  RTK integer AR is unreliable without ionospheric modelling.
- **MADOCA low N** in some runs (e.g. nagoya_run1: 1 968 vs 7 525 for CLAS)
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
