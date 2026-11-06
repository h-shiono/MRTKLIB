# Quick Start: BeiDou PPP-B2b

This guide shows how to run **BeiDou PPP-B2b** float PPP with MRTKLIB, in
post-processing and in real time.

PPP-B2b is a **free** BeiDou service. The four BDS-3 GEO satellites broadcast
SSR corrections (satellite orbit, clock and code bias) on the **B2b-I** signal
(1207.14 MHz). Coverage is China and the surrounding region; all four GEOs are
visible from Japan. The corrections are applied to the broadcast ephemeris.

!!! note "BeiDou-only float PPP"
    PPP-B2b broadcasts no phase biases and no ionosphere / troposphere
    corrections, so MRTKLIB runs it as **float** PPP, like Galileo HAS. The
    broadcast also has orbit and clock corrections for GPS but no GPS code
    biases. Without them MRTKLIB drops GPS pseudoranges, so the bundled
    configurations use **BeiDou only**.

---

## Prerequisites

- MRTKLIB built (`cmake --build build`).
- A receiver that tracks **B2b on the BDS-3 GEOs** and logs them in SBF, for
  example a Septentrio mosaic-G5 or mosaic-X5. Enable B2b tracking, and check
  that the GEOs are actually tracked: they are **C01–C04** since the April
  2026 PRN reassignment.
- SBF blocks:

| Block | Name | Needed for |
|:------|:-----|:-----------|
| 4242 | BDSRawB2b | the PPP-B2b messages (post and real time) |
| 4251 | BDSCNav1 | the B-CNAV1 ephemeris the corrections refer to (real time) |
| 4027 | MeasEpoch | observations |

---

## Required Files (post-processing)

| File | Purpose |
|:-----|:--------|
| RINEX observation (`.obs`) | observations, from the SBF with `mrtk convert` |
| RINEX **4** navigation with BeiDou **CNV1** records | broadcast ephemeris the corrections refer to |
| PPP-B2b messages (`.b2b`) | from the SBF with `mrtk l6extract` |

PPP-B2b orbit corrections refer to the **B-CNAV1** ephemeris (IODN = IODC of
B-CNAV1), not to the D1/D2 ephemeris. `mrtk convert` writes no CNV1 records,
so use a RINEX 4 broadcast file that has them, e.g. the IGS merged file
`BRD400DLR_S_YYYYDDD0000_01D_MN.rnx`. The RINEX 3 `BRDC00WRD` file has no CNV1
records and cannot be used.

---

## Bundled Configuration Files

| File | Mode | Use |
|:-----|:-----|:----|
| `conf/b2b/ppp_bds_b2b.toml` | Float PPP (`ppp-kine`) | Post-processing |
| `conf/b2b/run_bds_b2b.toml` | Float PPP | Real-time (`mrtk run`) |

Both set `correction = "bds-b2b"`, `satellite_ephemeris = "brdc+ssrapc"`,
`systems = ["BeiDou"]` and the BeiDou PRN lists for data after 2026-04-21.

---

## Post-Processing

```bash
# 1. Extract the PPP-B2b messages of the GEOs -> rover.b2b
mrtk l6extract rover.sbf

# 2. Convert the SBF to RINEX observations
mrtk convert -r sbf -o rover.obs -n rover.nav rover.sbf

# 3. Float PPP with PPP-B2b
mrtk post -k conf/b2b/ppp_bds_b2b.toml -o rover.pos \
  rover.obs BRD400DLR_S_20262960000_01D_MN.rnx rover.b2b
```

`mrtk l6extract` writes `<input-basename>.b2b` with the frames of the GEOs in
`-b2b-geo` (default `C01-C04`). `mrtk post` detects the `.b2b` input by its
extension and selects the B-CNAV1 ephemeris for BeiDou.

---

## Real-Time

```bash
mrtk run -o conf/b2b/run_bds_b2b.toml
```

Edit `[streams.input.rover] path` first. A single SBF stream gives the
observations, the PPP-B2b messages and the B-CNAV1 ephemeris; no separate
correction stream is needed. To replay a recorded file in real time, use its
plain path with `::T::x<speed>` and a `.tag` file next to it.

The receiver outputs `BDSCNav1` only when a new ephemeris set arrives. A
replay that starts within the hour can therefore run without BeiDou
ephemeris until the next update.

---

## Key Configuration Points

### Correction source

```toml
[positioning]
correction = "bds-b2b"
satellite_ephemeris = "brdc+ssrapc"
```

`correction = "bds-b2b"` must be paired with `brdc+ssrapc` (or `brdc+ssrcom`);
other ephemeris settings are rejected.

### BeiDou PRNs

```toml
[positioning]
systems = ["BeiDou"]
bds2_sats = ["C09", "C10"]     # BDS-2 in PRN 1-18, data after 2026-04-21
bds_geo_sats = ["C01-C04"]     # PPP-B2b GEOs, data after 2026-04-21
```

For data before 2026-04-10 use `bds2_sats = ["C01-C18"]` and
`bds_geo_sats = ["C59-C62"]`. See the
[BeiDou PRN Reassignment guide](beidou-prn-reassignment.md).

### GEO streams

The GEOs do not all send the same correction stream. On 2026-10-23, C01–C03
sent IOD SSR 1 and C04 IOD SSR 2. MRTKLIB decodes one GEO at a time: the first
that sends a satellite mask, until it has been silent for 10 s.

### Signals

```toml
[signals]
bds3 = "B1I/B3I"
```

B3I is the clock reference of the broadcast. The iono-free combination handles
the ionosphere, and the troposphere is estimated (`est-ztd`).

---

## What Accuracy to Expect

BeiDou-only float PPP, mosaic-G5 with a TOPCON CR-G5 antenna in Tokyo,
2026-10-23 20:00–22:00 GPST, against a PPP-AR reference coordinate. Statistics
after the first 60 min:

| | 2D rms | U rms | 3D 95 % |
|:--|:-:|:-:|:-:|
| Post, static | 1.8 cm | 3.9 cm | 6.4 cm |
| Post, kinematic | 15.8 cm | 10.8 cm | 30.9 cm |
| Real-time replay, kinematic | 14.8 cm | 10.7 cm | 27.9 cm |

About 9 BeiDou satellites were used per epoch. These numbers come from one
2-hour session and are not a service performance figure. A float solution
takes tens of minutes to converge.

---

## Limitations

- **Float PPP only.** No phase biases are broadcast.
- **BeiDou only.** The broadcast has no GPS code biases.
- **No ionosphere / troposphere corrections** (estimated locally).
- **Regional.** Coverage is China and the surrounding area.

---

## Troubleshooting

### The `.b2b` file is empty

`mrtk l6extract` prints the `BDSRawB2b` blocks it saw and the GEO records it
wrote.

- **Few `BDSRawB2b` blocks.** B2b is not tracked or not logged; enable B2b
  tracking and SBF block 4242.
- **Blocks present but no GEO records.** The frames are from IGSO / MEO
  satellites (B-CNAV3 navigation), so the GEOs are not tracked or not in
  `-b2b-geo`. Check the data date: C59–C62 before the reassignment, C01–C04
  after it.

### No BeiDou satellites in the solution

- **Post-processing:** the navigation file has no BeiDou CNV1 records. Use a
  RINEX 4 file such as `BRD400DLR_S`.
- **Real time:** SBF block 4251 (BDSCNav1) is not logged.
- **Both:** `bds_geo_sats` does not match the data date, so no corrections
  are decoded.

---

## Next Steps

- [BeiDou PRN Reassignment](beidou-prn-reassignment.md) — `bds2_sats` / `bds_geo_sats`
- [Quick Start: Galileo HAS PPP](quickstart-gal-has.md) — the other global float-PPP service
- [Configuration Options Reference](../reference/config-options.md) — full options list
