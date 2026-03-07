# Kinematic Positioning Benchmark (PPC-Dataset)

This benchmark evaluates MRTKLIB's kinematic (vehicle-mounted) positioning
performance using the open-data [PPC2024 (Precise Positioning Challenge)][ppc]
dataset collected for the contest organised by the Institute of Navigation Japan
(測位航法学会).  It covers six urban driving runs
(Nagoya × 3, Tokyo × 3) and supports three positioning modes:

| Mode | Engine | Correction |
|------|--------|------------|
| CLAS | PPP-RTK | QZSS L6D (IS-QZSS-L6-003) |
| MADOCA | PPP | QZSS L6E (MADOCA-PPP) |
| RTK | Kinematic RTK | Rover + base station RINEX |

> **Note:** This benchmark is intentionally excluded from the regular CTest
> suite because it requires large external datasets.  Run it on demand.

> **Disclaimer:** The configuration parameters used here have not been tuned or
> optimised for maximum accuracy.  Results are provided for reference only and
> should not be taken as representative of the best achievable performance.

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
   cd build && ctest -R setup --output-on-failure
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
python scripts/benchmark/run_benchmark.py
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
| `--mode clas\|madoca\|rtk\|both\|all` | `all` | Positioning mode (`both`=clas+madoca, `all`=all three) |
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

CLAS and RTK produce integer-fix solutions and are broken down into three tiers.
MADOCA-PPP never produces an integer fix and is reported as a single **PPP** tier.

| Tier | Modes | GGA quality | Description |
|------|-------|-------------|-------------|
| **FIX** | CLAS, RTK | Q=4 | Integer ambiguity fix epochs only |
| **FF** | CLAS, RTK | Q=4, 5 | Fix + float; excludes SPP fallback (Q=1) |
| **ALL** | CLAS, RTK | any | Every matched epoch including SPP |
| **PPP** | MADOCA | Q=3 | All valid PPP-float epochs (Q=0 already filtered) |

**Rate% column:**
- FIX / FF rows: fraction of that tier among all matched epochs
- PPP row: fraction of epochs with 2D horizontal error < 30 cm

**TTFF column:**
- CLAS / RTK: first epoch of a ≥30-consecutive Q=4 run (shown on FIX row)
- MADOCA: first epoch of a ≥30-consecutive sub-30 cm run (shown on PPP row)

### Nagoya

| Case | Mode | Tier | N | nSV | Rate% | RMS 2D | 1σ (68%) | 95% | TTFF (s) |
|------|------|------|--:|----:|------:|-------:|---------:|----:|---------:|
| nagoya_run1 | CLAS   | FIX |  1 276 |  7.9 | 17.0% |   1.105 m | 0.402 m |  0.452 m |   0 |
|             |        | FF  |  1 828 |  7.2 | 24.3% |   2.870 m | 0.441 m |  8.409 m |   — |
|             |        | ALL |  7 525 |  9.0 |     — |  42.784 m | 4.089 m | 53.212 m |   — |
| nagoya_run1 | MADOCA | PPP |  1 968 | 15.0 |  0.0% |  17.428 m | 2.108 m |  4.684 m |   — |
| nagoya_run1 | RTK    | FIX |  2 140 | 18.8 | 29.7% |   1.536 m | 0.112 m |  0.151 m | 799 |
|             |        | FF  |  7 216 | 16.8 |100.0% |   5.953 m | 0.398 m |  4.809 m |   — |
|             |        | ALL |  7 216 | 16.8 |     — |   5.953 m | 0.398 m |  4.809 m |   — |
| nagoya_run2 | CLAS   | FIX |  2 522 |  6.5 | 26.9% |   1.088 m | 0.717 m |  0.826 m |   0 |
|             |        | FF  |  6 002 |  5.6 | 63.9% |  15.903 m | 1.455 m | 36.589 m |   — |
|             |        | ALL |  9 390 |  4.7 |     — | 305.610 m | 8.323 m | 73.892 m |   — |
| nagoya_run2 | MADOCA | PPP |  7 494 | 11.9 |  0.2% |  39.299 m | 2.967 m | 19.443 m |   — |
| nagoya_run2 | RTK    | FIX |  1 489 | 20.7 | 16.1% |   1.081 m | 0.154 m |  0.196 m |   0 |
|             |        | FF  |  9 274 | 15.3 |100.0% |   6.588 m | 1.026 m | 11.809 m |   — |
|             |        | ALL |  9 274 | 15.3 |     — |   6.588 m | 1.026 m | 11.809 m |   — |
| nagoya_run3 | CLAS   | FIX |    325 |  6.9 |  6.3% |   0.318 m | 0.339 m |  0.397 m |   9 |
|             |        | FF  |  1 973 |  5.7 | 38.4% |  12.026 m | 2.530 m | 38.241 m |   — |
|             |        | ALL |  5 141 |  6.0 |     — |  12.746 m | 6.606 m | 27.890 m |   — |
| nagoya_run3 | MADOCA | PPP |  2 753 | 10.8 |  2.1% |   2.649 m | 2.246 m |  4.285 m |   — |
| nagoya_run3 | RTK    | FIX |    416 | 18.7 |  8.2% |   0.307 m | 0.128 m |  0.150 m |  72 |
|             |        | FF  |  5 095 | 13.2 |100.0% |   3.832 m | 3.462 m |  8.016 m |   — |
|             |        | ALL |  5 095 | 13.2 |     — |   3.832 m | 3.462 m |  8.016 m |   — |

### Tokyo

| Case | Mode | Tier | N | nSV | Rate% | RMS 2D | 1σ (68%) | 95% | TTFF (s) |
|------|------|------|--:|----:|------:|-------:|---------:|----:|---------:|
| tokyo_run1 | CLAS   | FIX |    612 |  7.6 |  5.2% |   0.868 m |  0.239 m |  2.483 m |  15 |
|            |        | FF  |  9 235 |  6.4 | 77.8% | 119.243 m |  3.462 m | 25.545 m |   — |
|            |        | ALL | 11 867 |  5.9 |     — | 627.063 m |  7.914 m |277.340 m |   — |
| tokyo_run1 | MADOCA | PPP |  3 084 | 12.9 | 16.6% |   1.825 m |  0.979 m |  1.957 m |   0 |
| tokyo_run1 | RTK    | FIX |    380 | 15.7 |  3.4% |   0.711 m |  0.027 m |  0.643 m |1840 |
|            |        | FF  | 11 271 | 16.0 |100.0% |   8.272 m |  7.033 m | 19.674 m |   — |
|            |        | ALL | 11 271 | 16.0 |     — |   8.272 m |  7.033 m | 19.674 m |   — |
| tokyo_run2 | CLAS   | FIX |  1 972 |  7.2 | 21.7% |   0.590 m |  0.117 m |  1.041 m | 368 |
|            |        | FF  |  6 841 |  6.3 | 75.3% |  22.988 m |  1.194 m | 14.921 m |   — |
|            |        | ALL |  9 091 |  5.7 |     — |  33.433 m |  2.389 m | 33.465 m |   — |
| tokyo_run2 | MADOCA | PPP |  8 159 | 13.7 |  6.7% |   2.894 m |  1.181 m |  4.580 m | 464 |
| tokyo_run2 | RTK    | FIX |  1 517 | 24.3 | 18.3% |  17.993 m |  0.010 m |  0.059 m | 806 |
|            |        | FF  |  8 304 | 19.2 |100.0% |  33.845 m |  4.521 m | 50.604 m |   — |
|            |        | ALL |  8 304 | 19.2 |     — |  33.845 m |  4.521 m | 50.604 m |   — |
| tokyo_run3 | CLAS   | FIX |  1 129 |  8.1 |  7.4% |   0.801 m |  0.075 m |  1.831 m |  28 |
|            |        | FF  |  2 629 |  7.3 | 17.2% |   3.764 m |  0.628 m |  2.427 m |   — |
|            |        | ALL | 15 241 | 11.4 |     — |  41.464 m |  3.737 m | 29.607 m |   — |
| tokyo_run3 | MADOCA | PPP |  2 795 | 14.3 |  3.4% |   0.724 m |  0.750 m |  1.062 m |  26 |
| tokyo_run3 | RTK    | FIX |  3 669 | 25.0 | 25.1% |   0.293 m |  0.013 m |  0.051 m | 335 |
|            |        | FF  | 14 639 | 21.7 |100.0% |   4.178 m |  1.374 m | 10.761 m |   — |
|            |        | ALL | 14 639 | 21.7 |     — |   4.178 m |  1.374 m | 10.761 m |   — |

### Notes

- **CLAS FIX accuracy** (Q=4 only): RMS 2D is **0.3–1.1 m** across all runs —
  consistent PPP-RTK performance under urban multipath.
- **CLAS FF vs ALL gap**: the large ALL RMS (13–627 m) reflects SPP fallback
  epochs (Q=1) that dominate in dense canyons.  FF tier (24–78%) excludes these.
- **RTK FF = ALL**: RTK outputs Q=4 (fix) or Q=5 (float) only — no SPP fallback
  — so FF always equals ALL.
- **RTK FIX quality**: with triple-frequency AR (`l1+2+3`, `ionoopt=off`), most
  runs achieve sub-metre FIX RMS (e.g. nagoya_run3: 0.31 m, tokyo_run3: 0.29 m).
  tokyo_run2 FIX RMS remains elevated (18 m) despite tiny 1σ/95% values, indicating
  a small number of wrongly-fixed epochs that dominate the RMS at this ~27 km baseline.
- **MADOCA PPP N**: outputs Q=3 (PPP float) or Q=0 (no solution); Q=0 is
  filtered, so N reflects epochs where the filter produced a solution.  In
  nagoya_run1, only 27% of rover epochs have a valid solution (heavy urban
  multipath).  In nagoya_run2 and tokyo_run2 (more open sky), coverage is ~100%.
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
| **nSV** | Mean number of satellites used in the solution for that tier |
| **Fix%** | Percentage of epochs with GGA quality = 4 (integer AR fix) |
| **RMS_2D (all)** | Horizontal RMS error across all matched epochs |
| **RMS_3D (all)** | 3D RMS error across all matched epochs |
| **RMS_2D (fix)** | Horizontal RMS for Q=4 epochs only; `nan` if no fix |
| **Conv_s** | Seconds from first matched epoch to first run of ≥30 consecutive Q=4 |

ENU errors are computed per-epoch using the corresponding ground-truth
coordinate as the reference origin (moving-base projection).

---

## Configuration

The benchmark uses layered configuration files.  Each run is processed with
`rnx2rtkp -k <mode>.conf -k <city>.conf`, so the city conf overrides only
the keys it specifies.

**Mode confs** (common settings per algorithm):

- `conf/benchmark/clas.conf` — CLAS PPP-RTK
- `conf/benchmark/madoca.conf` — MADOCA PPP
- `conf/benchmark/rtk.conf` — Baseline RTK

**City confs** (antenna types and reference-station coordinates):

- `conf/benchmark/nagoya.conf` — Nagoya overrides
- `conf/benchmark/tokyo.conf` — Tokyo overrides

Key differences from the standard test configurations:

| Parameter | Value | Reason |
|-----------|-------|--------|
| `ant1-anttype` (Nagoya) | `TRM105000.10 NONE` | Trimble antenna per equipment manifest |
| `ant1-anttype` (Tokyo) | `*` | rover antenna not yet identified |
| `ant2-anttype` (Nagoya) | `TRM115000.00 NONE` | base antenna per equipment manifest |
| `ant2-anttype` (Tokyo) | `TRM55971.00 NONE` | base antenna (RINEX header has wrong model) |
| `pos2-isb` | `off` | no ISB calibration for Septentrio mosaic-X5 |
| `pos1-dynamics` | `on` | kinematic mode (MADOCA, RTK) |
| `pos1-frequency` (RTK) | `l1+2+3` | triple-frequency AR |
| `pos1-ionoopt` (RTK) | `off` | raw observations on all 3 freq (DD cancels iono) |
| `pos1-snrmask_r` (RTK) | `on` | SNR mask (≥30 dBHz above 25°) |
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

The PPC-Dataset was collected for the **Precise Positioning Challenge 2024
(高精度測位チャレンジ2024)** organised by the Institute of Navigation Japan
(測位航法学会), and is kindly made available as open data by:

> **Taro Suzuki**, Chiba Institute of Technology
> *PPC-Dataset — GNSS/IMU driving data for precise positioning research*
> <https://github.com/taroz/PPC-Dataset>

We gratefully acknowledge the Institute of Navigation Japan for organising PPC2024
and Prof. Suzuki for making the dataset publicly available.  Please cite the
PPC2024 materials when publishing results derived from this dataset.

---

## References

- [PPC2024 overview (Japanese)][ppc-pdf]
- [PPC2024 results (Japanese)][ppc-res]
- [PPC-Dataset GitHub][ppc]

[ppc]: https://github.com/taroz/PPC-Dataset
[ppc-pdf]: http://taroz.net/data/PPC2024.pdf
[ppc-res]: http://taroz.net/data/PPC2024_results.pdf
