# Quick Start: Galileo HAS PPP

This guide walks you through running **Galileo High Accuracy Service (HAS)**
float PPP with MRTKLIB, in both post-processing and real-time.

Galileo HAS is a **free, global** high-accuracy service that broadcasts SSR
corrections (satellite orbit, clock, and code bias) for **GPS and Galileo** on
the **E6-B C/NAV** signal (and over the internet). Corrections are referenced to
the **GTRF/ITRF2020** frame. Combined with broadcast ephemeris, HAS enables
decimeter-level global PPP without a regional service area.

!!! note "Float PPP only (HAS Initial Service)"
    The HAS **Initial Service** broadcasts no phase biases and no
    ionosphere/troposphere corrections, so MRTKLIB runs HAS as **float** PPP
    (no ambiguity resolution), analogous to the MADOCA-PPP float path. Integer
    PPP-AR is planned for when HAS reaches Full Operational Capability (FOC)
    with phase biases.

---

## Prerequisites

- MRTKLIB built successfully (`cmake --build build`)
- A receiver that tracks and outputs **Galileo E6-B C/NAV**, e.g. a
  **Septentrio mosaic-G5**, configured to log SBF **GALRawCNAV** (block 4024)
  alongside the usual measurement and raw-ephemeris blocks needed for RINEX
  conversion.

## Required Files

| File | TOML Key | Purpose |
|:-----|:---------|:--------|
| RINEX observation (`.obs`) | positional arg | GNSS observations from the receiver |
| RINEX navigation (`.nav` / `.rnx`) | positional arg | Broadcast ephemeris (GPS + Galileo) |
| HAS corrections (`.has`) | positional arg | Galileo HAS SSR (orbit, clock, code bias) |

The `.has` file is produced from the SBF capture by `mrtk l6extract` (see
below). For post-processing, the observations and navigation come from the same
SBF capture converted with `mrtk convert`.

---

## Bundled Configuration Files

| File | Mode | Use |
|:-----|:-----|:----|
| `conf/has/ppp_gal_has.toml` | Float PPP (`ppp-kine`) | Post-processing |
| `conf/has/run_gal_has.toml` | Float PPP | Real-time (`mrtk run`) |

Both set `correction = "gal-has"` and `satellite_ephemeris = "brdc+ssrapc"`
(HAS SSR applied to broadcast ephemeris) and restrict `systems` to GPS and
Galileo — the constellations HAS corrects.

---

## Post-Processing

```bash
# 1. Extract HAS pages from the SBF capture
mrtk l6extract -i G5P3162a.sbf -o G5P3162a.has

# 2. Convert the SBF to RINEX obs/nav (same capture)
mrtk convert ... -o G5P3162a.obs -n G5P3162a.nav G5P3162a.sbf

# 3. Float PPP with HAS corrections
mrtk post -k conf/has/ppp_gal_has.toml \
  G5P3162a.obs G5P3162a.nav G5P3162a.has
```

`mrtk post` detects the `.has` infile by its extension, feeds its records to the
HAS decoder, and applies the decoded corrections through the SSR branch.

**Expected output:** float PPP solutions (Q=6) on every epoch once HAS
corrections are available.

---

## Real-Time

```bash
mrtk run -o conf/has/run_gal_has.toml
```

Edit `[streams.input.rover] path` first. To replay a recorded file in real
time, use its plain path with `::T::x<speed>` and a `.tag` file next to it.

The real-time path takes a **single SBF stream**: the Septentrio decoder routes
GALRawCNAV pages to the HAS decoder while the same stream supplies observations
and broadcast ephemeris. No separate correction stream is required.

---

## Key Configuration Points

### Correction Source

```toml
[positioning]
correction = "gal-has"
satellite_ephemeris = "brdc+ssrapc"
```

`gal-has` routes corrections through the SSR branch; `brdc+ssrapc` applies the
HAS orbit/clock SSR to broadcast ephemeris (antenna-phase-center convention).

!!! warning "`gal-has` requires `brdc+ssrapc`"
    `correction = "gal-has"` must be paired with
    `satellite_ephemeris = "brdc+ssrapc"`. With any other ephemeris source the
    SSR corrections are not applied and you will get no usable HAS solution.

### Constellations and Signals

```toml
[positioning]
systems = ["GPS", "Galileo"]
```

HAS corrects GPS and Galileo only. The iono-free combination is used for the
ionosphere (`dual-freq`) and the troposphere is estimated (`est-ztd`); HAS
carries neither correction itself.

---

## What Accuracy to Expect

| Metric | Result |
|:-------|:-------|
| **Repeatability** (1 h self-RMS) | E 7.5 / N 7.3 / U 38 cm |
| **Absolute** (vs cm-grade ITRF reference) | ~0.5–0.6 m horizontal (investigation open) |
| **Convergence** | decimeter-level after the float filter settles |

!!! note "Absolute accuracy is under investigation"
    The HAS float solution currently sits ~0.5–0.6 m horizontal from a cm-grade
    ITRF reference — larger than mature PPP would give. The accuracy
    investigation is open ([#215](https://github.com/h-shiono/MRTKLIB/issues/215));
    published HAS Asia-region performance suggests 10–20 cm is attainable, so day-to-day
    repeatability (above) is the more representative figure for now.

---

## Limitations

- **Float PPP only** — no phase biases in the Initial Service, so no PPP-AR yet.
- **No ionosphere / troposphere corrections** in HAS (estimated locally instead).
- **GPS + Galileo only.**

---

## Troubleshooting

### `correction = "gal-has"` rejected / no corrections applied

Ensure `satellite_ephemeris = "brdc+ssrapc"`. HAS is an SSR service applied to
broadcast ephemeris; another ephemeris source disables the SSR application.

### No solutions (stuck at Single / no fix in PPP)

1. **No HAS pages in the capture** — confirm the receiver was logging SBF
   **GALRawCNAV** (block 4024). Run `mrtk l6extract` and check that the `.has`
   file contains records; an empty or tiny `.has` means E6-B was not being
   tracked/decoded.
2. **E6-B not tracked** — verify the receiver is configured to track Galileo E6
   and output C/NAV. mosaic-G5 must have E6-B enabled.
3. **Missing GPS/Galileo broadcast ephemeris** — the converted `.nav` must
   contain GPS and Galileo ephemerides; HAS corrects orbit/clock relative to
   broadcast ephemeris.

### Larger-than-expected position offset

This is the known absolute-accuracy offset (~0.5–0.6 m horizontal) under
investigation in [#215](https://github.com/h-shiono/MRTKLIB/issues/215).
Repeatability is good; absolute accuracy is expected to improve.

---

## Next Steps

- [Configuration Options Reference](../reference/config-options.md) — full options list
- [Quick Start: MADOCA-PPP](quickstart-madoca-ppp.md) — global PPP via QZSS L6E
- [Configuration Guide](configuration.md) — TOML format overview
