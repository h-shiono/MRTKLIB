# Quick Start: CLAS PPP-RTK

This guide walks you through running CLAS PPP-RTK positioning with MRTKLIB,
from required files to your first solution.

CLAS (Centimeter Level Augmentation Service) is a satellite-based augmentation
service broadcast via QZSS L6D. It provides ionospheric, tropospheric, and
satellite bias corrections that enable centimeter-level PPP-RTK positioning
within Japan.

---

## Prerequisites

- MRTKLIB built successfully (`cmake --build build`)
- Test data extracted:

```bash
tar xzf tests/data/claslib/claslib_testdata.tar.gz -C tests/data/claslib/
```

## Required Files

CLAS PPP-RTK requires several support files beyond the usual observation
and navigation data. All files below are **bundled** with MRTKLIB.

| File | TOML Key | Purpose |
|:-----|:---------|:--------|
| `clas_grid.def` | `files.cssr_grid` | CLAS grid point definitions covering Japan. **Required** — without this, no corrections are applied. |
| `clas_grid.blq` | `files.ocean_loading` | Ocean tide loading coefficients at each grid point. |
| `igu00p01.erp` | `files.eop` | Earth rotation parameters for coordinate frame transformation. |
| `igs14_L5copy.atx` | `files.receiver_atx` | Antenna phase center model (receiver and satellite). |
| `isb.tbl` | `files.isb_table` | Inter-system bias table mapping receiver types to CLAS reference. |
| `l2csft.tbl` | `files.phase_cycle` | L2C quarter-cycle phase shift correction per receiver type. |

All bundled files are located in `tests/data/claslib/`.

!!! warning "ANTEX File: Use IGS14, Not IGS20"
    CLAS corrections are based on the **ITRF2014** reference frame. You must
    use an IGS14-based ANTEX file (e.g., `igs14.atx` or `igs14_L5copy.atx`).

    The latest IGS20-based ANTEX (`igs20.atx`) uses ITRF2020, which introduces
    millimeter-to-centimeter frame differences that degrade positioning accuracy.

---

## Post-Processing

### Run with Bundled Test Data

```bash
mrtk post -k conf/claslib/rnx2rtkp.toml \
  -ts 2019/08/27 16:00:00 -te 2019/08/27 17:00:00 -ti 1 \
  -o output_clas.pos \
  tests/data/claslib/0627239Q.obs \
  tests/data/claslib/0627239Q.nav \
  tests/data/claslib/2019239Q.l6
```

- **Observation file** (`.obs` / `.bnx`) — RINEX or BINEX from a GNSS receiver in Japan
- **Navigation file** (`.nav`) — Broadcast ephemeris
- **L6 correction file** (`.l6`) — CLAS L6D data (extracted from QZSS L6 signal)

### Expected Output

After ~1 minute of convergence, the solution should reach **Fix (Q=4)**:

```
$GNGGA,160131.00,3908.109031,N,14107.972285,E,4,08,...
```

- **Q=4 (Fix)** — Ambiguities resolved, centimeter-level accuracy
- **Q=5 (Float)** — Convergence in progress
- **Q=1 (SPP)** — Initial convergence or missing corrections

Typical fix rate for this dataset: **>99%** (after initial convergence).

---

## Real-Time Processing

For real-time CLAS PPP-RTK, use `mrtk run` with live receiver streams.
See [Real-Time CLAS Reference](../reference/rtkrcv-clas-realtime.md) for
architecture details, stream configuration, and performance data.

### File Replay (Testing)

To test real-time processing with recorded data:

```bash
mrtk run -s -o conf/claslib/rtkrcv.toml
```

The bundled `rtkrcv.toml` is pre-configured for file replay using the
test dataset with time-tag synchronization.

### Live Receiver

For a live u-blox F9P + D9C setup, use the template config:

```bash
mrtk run -s -o conf/claslib/rtkrcv_ubx_clas.toml
```

Edit the `[streams.input.*]` sections to match your receiver connections.

---

## Key Configuration Points

These settings are critical for correct CLAS PPP-RTK operation. Getting
any of them wrong will silently degrade or prevent Fix solutions.

### Frequency: Must Be `l1+2`

```toml
[positioning]
frequency = "l1+2"   # REQUIRED for CLAS
```

CLAS provides E1 and E5a corrections but does not provide Galileo E5b
phase bias. Using `l1+2+3` (nf=3) adds the E5b slot, which has no valid
bias correction, causing false geometry-free cycle slips on Galileo and
dropping fix rate from >99% to ~67%.

### Ionosphere: `est-adaptive` (Not `dual-freq`)

```toml
[positioning.atmosphere]
ionosphere = "est-adaptive"   # CLAS provides iono corrections
troposphere = "off"           # CLAS provides tropo corrections
```

CLAS supplies ionospheric and tropospheric corrections via the grid. The
Kalman filter estimates residual ionospheric delays adaptively.
Do not use `dual-freq` — that discards the CLAS ionospheric information.

### Tidal Correction: CLAS Grid-Based

```toml
[positioning.corrections]
tidal_correction = "solid+otl-clasgrid+pole"
```

This uses the CLAS grid points for ocean tide loading interpolation,
rather than the station-specific BLQ method.

### ISB and Phase Shift: Enabled with Tables

```toml
[receiver]
isb = true
phase_shift = "table"
reference_type = "CLAS"
```

CLAS corrections are referenced to a specific receiver type. The ISB
table and phase cycle table correct for inter-system biases and L2C
quarter-cycle shifts between your receiver and the CLAS reference.

!!! note "Receiver Type"
    Set `[positioning.clas].receiver_type` to match your GNSS receiver
    (e.g., `"Trimble NetR9"`). This value is used for ISB table lookup.
    Check `isb.tbl` for supported receiver types.

### Grid Selection Radius

```toml
[positioning.clas]
grid_selection_radius = 1000   # meters
```

Controls how far from the rover position to search for CLAS grid points.
1000 m is appropriate for most locations within Japan.

!!! warning "Japan Coverage Only"
    CLAS grid corrections are only available within Japan. Processing
    observations from outside the coverage area will fail silently
    (grid lookup returns no results).

---

## Bundled Configuration Files

| File | Description |
|:-----|:------------|
| `conf/claslib/rnx2rtkp.toml` | Post-processing, single-channel L6 |
| `conf/claslib/rnx2rtkp_vrs.toml` | Post-processing, VRS-RTK mode |
| `conf/claslib/rtkrcv.toml` | Real-time, single-channel L6 (file replay) |
| `conf/claslib/rtkrcv_2ch.toml` | Real-time, dual-channel L6 (file replay) |
| `conf/claslib/rtkrcv_ubx_clas.toml` | Real-time, u-blox F9P + D9C (live) |
| `conf/claslib/rtkrcv_sbf_l6d.toml` | Real-time, Septentrio SBF + L6D |
| `conf/claslib/rtkrcv_rtcm3_ubx.toml` | Real-time, RTCM3 + UBX L6 |

---

## Troubleshooting

### All Solutions Are SPP (Q=1)

1. **Missing grid file** — Check that `files.cssr_grid` points to a valid `clas_grid.def`
2. **Wrong frequency** — Ensure `frequency = "l1+2"`, not `l1+2+3`
3. **No L6 data** — Verify the L6 correction file path and format

### Fix Rate Lower Than Expected

1. **nf=3 instead of nf=2** — Most common cause. See [Frequency](#frequency-must-be-l1-2) above.
2. **Wrong ANTEX** — Using `igs20.atx` instead of `igs14.atx` introduces frame errors.
3. **Receiver type mismatch** — Check `[positioning.clas].receiver_type` matches `isb.tbl`.

### `clas grid file error`

The grid file path is invalid or the file doesn't exist. Check the
`files.cssr_grid` setting. Relative paths are resolved from the
working directory, not from the config file location.

---

## Next Steps

- [Configuration Options Reference](../reference/config-options.md) — Full options list with mode applicability
- [Real-Time CLAS Reference](../reference/rtkrcv-clas-realtime.md) — Architecture, performance data, tag generation
- [Configuration Guide](configuration.md) — TOML format overview
