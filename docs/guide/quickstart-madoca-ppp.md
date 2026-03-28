# Quick Start: MADOCA-PPP

This guide walks you through running MADOCA-PPP (Precise Point Positioning)
with MRTKLIB, covering both basic PPP and PPP with ambiguity resolution (PPP-AR).

MADOCA (Multi-GNSS Advanced Demonstration tool for Orbit and Clock Analysis)
provides real-time satellite orbit, clock, and bias corrections via QZSS L6E.
Combined with broadcast ephemeris, it enables decimeter-to-centimeter-level
global PPP positioning.

---

## Prerequisites

- MRTKLIB built successfully (`cmake --build build`)
- Test data extracted:

```bash
tar xzf tests/data/madocalib/madocalib_testdata.tar.gz -C tests/data/madocalib/
```

## Required Files

| File | TOML Key | Purpose |
|:-----|:---------|:--------|
| RINEX observation (`.obs`) | positional arg | GNSS observations from a receiver |
| RINEX navigation (`.rnx` / `.nav`) | positional arg | Broadcast ephemeris (mixed GNSS) |
| L6E correction (`.l6`) | positional arg | MADOCA SSR corrections (orbit, clock, bias) |
| Antenna model (`.atx`) | `files.receiver_atx` | Receiver/satellite antenna phase center model |

!!! note "ANTEX File: IGS20 for MADOCA"
    Unlike CLAS (which requires IGS14), MADOCA-PPP uses the **IGS20/ITRF2020**
    reference frame. Use `igs20.atx` for antenna corrections.

### Optional Files (for PPP-AR)

For ambiguity-resolved PPP, no additional files are required beyond the L6E
corrections — MADOCA provides the necessary phase bias information via SSR.

---

## Bundled Configuration Files

MRTKLIB provides three MADOCA-PPP configurations with increasing capability:

| File | Mode | AR | Iono | Frequencies |
|:-----|:-----|:---|:-----|:------------|
| `conf/madocalib/rnx2rtkp.toml` | PPP | Off | dual-freq | L1+L2 |
| `conf/madocalib/rnx2rtkp_pppar.toml` | PPP-AR | Continuous | est-stec | L1+L2+L3+L4 |
| `conf/madocalib/rnx2rtkp_pppar_iono.toml` | PPP-AR + Iono | Continuous | est-stec | L1+L2+L3+L4 |

---

## Post-Processing

### Basic PPP (Float Solution)

The simplest mode — no ambiguity resolution, dual-frequency ionospheric
correction:

```bash
mrtk post -k conf/madocalib/rnx2rtkp.toml \
  -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30 \
  -o output_madoca_ppp.pos \
  tests/data/madocalib/MIZU00JPN_R_20250910000_01D_30S_MO.obs \
  tests/data/madocalib/BRDM00DLR_S_20250910000_01D_MN.rnx \
  tests/data/madocalib/2025091A.204.l6 \
  tests/data/madocalib/2025091A.206.l6
```

!!! tip "Multiple L6E Files"
    MADOCA uses two L6E streams (mdc-003 and mdc-004) on different PRNs.
    Pass both `.l6` files as positional arguments. The decoder selects the
    appropriate stream automatically.

**Expected accuracy:** Decimeter-level (Q=5 Float).

### PPP-AR (Ambiguity Resolution)

For centimeter-level accuracy with integer ambiguity resolution:

```bash
mrtk post -k conf/madocalib/rnx2rtkp_pppar.toml \
  -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30 \
  -o output_madoca_pppar.pos \
  tests/data/madocalib/MIZU00JPN_R_20250910000_01D_30S_MO.obs \
  tests/data/madocalib/BRDM00DLR_S_20250910000_01D_MN.rnx \
  tests/data/madocalib/2025091A.204.l6 \
  tests/data/madocalib/2025091A.206.l6
```

Key differences from basic PPP:

- `frequency = "l1+2+3+4"` — Multi-frequency processing
- `ambiguity_resolution.mode = "continuous"` — AR enabled
- `ionosphere = "est-stec"` — Slant TEC estimation (better than dual-freq for AR)
- `systems` includes all 5 constellations (GPS, GLONASS, Galileo, QZSS, BeiDou)

**Expected accuracy:** Centimeter-level after convergence (Q=1 Fix).

### PPP-AR with Ionospheric Correction

The highest-accuracy mode, using MADOCA ionospheric corrections:

```bash
mrtk post -k conf/madocalib/rnx2rtkp_pppar_iono.toml \
  -ts 2025/04/01 0:00:00 -te 2025/04/01 0:59:30 -ti 30 \
  -o output_madoca_pppar_iono.pos \
  tests/data/madocalib/MIZU00JPN_R_20250910000_01D_30S_MO.obs \
  tests/data/madocalib/BRDM00DLR_S_20250910000_01D_MN.rnx \
  tests/data/madocalib/2025091A.204.l6 \
  tests/data/madocalib/2025091A.206.l6
```

This adds `receiver.iono_correction = true`, enabling MADOCA-specific
ionospheric corrections for faster convergence.

---

## Key Configuration Points

### Ephemeris Source

```toml
[positioning]
satellite_ephemeris = "brdc+ssrapc"
```

MADOCA provides SSR corrections that are applied to broadcast ephemeris.
`brdc+ssrapc` uses antenna phase center-corrected SSR orbits.

### Signal Selection

```toml
[signals]
gps = "L1/L2/L5"
qzs = "L1/L5/L2"
galileo = "E1/E5a/E5b/E6"
bds2 = "B1I/B3I/B2I"
bds3 = "B1I/B3I/B2a"
```

The `[signals]` section controls which frequency pairs are used for each
constellation. These settings affect which observations enter the PPP filter
and must match the available MADOCA bias corrections.

### AR Thresholds (PPP-AR)

```toml
[ambiguity_resolution.thresholds]
ratio = 1.5     # LAMBDA validation ratio
ratio1 = 1.5    # Max 3D position std-dev to start narrow-lane AR (m)
```

`ratio1` gates AR activation — narrow-lane integer search only begins when
the float solution's 3D position standard deviation drops below this value.
This prevents premature AR attempts during initial convergence.

### Measurement Error Model

```toml
[kalman_filter.measurement_error]
code_phase_ratio_L1 = 300.0
code_phase_ratio_L2 = 300.0
ura_ratio = 0.1
```

- **Code/phase ratio** of 300 (vs 100 for RTK) reflects PPP's heavier
  reliance on carrier phase measurements.
- **URA ratio** scales satellite weighting by broadcast User Range Accuracy.

---

## Comparison: MADOCA-PPP vs CLAS PPP-RTK

| | MADOCA-PPP | CLAS PPP-RTK |
|:---|:-----------|:-------------|
| **Coverage** | Global | Japan only |
| **Signal** | QZSS L6E | QZSS L6D |
| **Convergence** | 10-30 minutes | 1-2 minutes |
| **Accuracy (converged)** | cm-level (PPP-AR) | cm-level |
| **Reference frame** | ITRF2020 (IGS20) | ITRF2014 (IGS14) |
| **ANTEX file** | `igs20.atx` | `igs14.atx` |
| **Grid files needed** | No | Yes (Japan grid) |
| **ISB table needed** | No | Yes |

---

## Troubleshooting

### No Convergence (Position Jumps)

1. **Wrong ANTEX** — Ensure `igs20.atx` is used (not `igs14.atx`).
2. **Missing L6E data** — Verify both L6E files are passed as arguments.
3. **Elevation mask too high** — 10 degrees is standard for PPP; higher
   values reduce satellite count and slow convergence.

### PPP-AR Not Fixing

1. **`ratio1` too tight** — If the float solution is noisy, AR won't
   activate. Try increasing `ratio1` to 2.0-3.0 m.
2. **Short observation span** — PPP-AR typically needs 15-30 minutes of
   data to converge and resolve ambiguities.
3. **Constellation limitation** — Ensure `systems` includes all available
   constellations for maximum satellite geometry.

---

## Next Steps

- [Configuration Options Reference](../reference/config-options.md) — Full options list with PPP-specific modes
- [Quick Start: CLAS PPP-RTK](quickstart-clas.md) — For Japan-specific centimeter-level positioning
- [Configuration Guide](configuration.md) — TOML format overview
