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

## Example Output

```
rnx2rtkp  : build/rnx2rtkp
Dataset   : data/benchmark
L6 cache  : data/benchmark/l6
Results   : data/benchmark/results
Mode      : both

[nagoya_run1  /  CLAS]
  rnx2rtkp completed in 28.3s → nagoya_run1_clas.nmea
  N=7240  Fix=91.3%  RMS_2D=0.412m  RMS_2D(fix)=0.063m  Conv=138s

...

--------------------------------------------------------------------------
Case                 Mode       N   Fix%  RMS_2D(all)  RMS_3D(all)  RMS_2D(fix)   Conv_s
--------------------------------------------------------------------------
nagoya_run1          clas    7240  91.3%     0.412 m      0.481 m      0.063 m      138
nagoya_run1          madoca  7240    --      0.354 m      0.413 m          nan      nan
...
```

*(Actual numbers depend on MRTKLIB version and L6 data availability.)*

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

## References

- [PPC2024 overview (Japanese)][ppc-pdf]
- [PPC2024 results (Japanese)][ppc-res]
- [PPC-Dataset GitHub][ppc]

[ppc]: https://github.com/taroz/PPC-Dataset
[ppc-pdf]: http://taroz.net/data/PPC2024.pdf
[ppc-res]: http://taroz.net/data/PPC2024_results.pdf
