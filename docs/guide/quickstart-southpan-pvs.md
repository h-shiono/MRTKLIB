# Quick Start: SouthPAN PVS

This guide shows how to run **SouthPAN PVS** (PPP via SouthPAN) float PPP with
MRTKLIB, in post-processing and in real time.

PVS is a **free** service of SouthPAN, the Australian / New Zealand SBAS. In
the current Initial Operational Capability it rides inside the DFMC SBAS L5
navigation data of the GEO satellite **PRN 122** (143.5°E): message type 32
(MT32) carries satellite orbit and clock corrections in ECEF, and an MT32 is
for PVS when its time of applicability `tD/16` is odd. The service area is
Australia and New Zealand; PRN 122 is also received in Japan, but the
corrections are computed only for the satellites SouthPAN monitors.

!!! note "GPS L1/L5 + Galileo E1/E5a float PPP"
    PVS broadcasts orbit and clock corrections only: no code or phase biases
    and no ionosphere / troposphere. MRTKLIB runs it as **float** iono-free
    PPP on GPS L1 C/A + L5 (LNAV ephemeris) and Galileo E1 + E5a (**F/NAV**
    ephemeris), the signals the corrections refer to. GPS satellites without
    L5 drop out.

---

## Prerequisites

- MRTKLIB built (`cmake --build build`).
- **Corrections**, one of:
    - a receiver that tracks the L5 signal of PRN 122 and logs it, for example
      a Septentrio mosaic-G5 or mosaic-X5 with `GEOL5` tracking and SBF block
      **4021 GEORawL5**;
    - a SouthPAN Data Access Service (DAS) log (SISNeT `*MSG` lines, L5 port),
      converted with `scripts/tests/das2sbs.py` (post-processing only for now).
- **Observations** with GPS L1 C/A + L5 and Galileo E1 + E5a, inside the
  service area for meaningful accuracy.
- SBF blocks for real time:

| Block | Name | Needed for |
|:------|:-----|:-----------|
| 4021 | GEORawL5 | the PVS messages (post and real time) |
| 4027 | MeasEpoch | observations |
| 5891 / 4017 | GPSNav / GPSRawCA | GPS LNAV ephemeris |
| 4002 / 4022 | GALNav / GALRawFNAV | Galileo F/NAV ephemeris |

---

## Required Files (post-processing)

| File | Purpose |
|:-----|:--------|
| RINEX observation (`.obs`) | observations |
| RINEX navigation with GPS LNAV and **Galileo F/NAV** records | broadcast ephemeris the corrections refer to |
| SBAS message file (`.sbs`) | PRN 122 L5 messages, from the SBF with `mrtk convert -s` or from a DAS log with `das2sbs.py` |

The Galileo corrections refer to the **F/NAV** ephemeris. The IGS merged
broadcast file `BRD400DLR_S_YYYYDDD0000_01D_MN.rnx` has F/NAV records, and so
does the navigation file `mrtk convert` writes from SBF `GALNav` blocks.

---

## Bundled Configuration Files

| File | Mode | Use |
|:-----|:-----|:----|
| `conf/pvs/ppp_southpan_pvs.toml` | Float PPP (`ppp-kine`) | Post-processing |
| `conf/pvs/run_southpan_pvs.toml` | Float PPP | Real-time (`mrtk run`) |

Both set `correction = "southpan-pvs"`, `satellite_ephemeris = "brdc+ssrapc"`,
`systems = ["GPS", "Galileo"]`, `[signals] gps = "L1/L5"` /
`galileo = "E1/E5a"` and `[input.sbas] satellite = 122`.

---

## Post-Processing

```bash
# 1. Convert the SBF to RINEX observations, navigation and SBAS messages
mrtk convert -r sbf -o rover.obs -n rover.nav -s rover.sbs rover.sbf

# 2. Float PPP with PVS
mrtk post -k conf/pvs/ppp_southpan_pvs.toml -o rover.pos \
  rover.obs rover.nav rover.sbs
```

With a DAS log instead of a receiver log:

```bash
python3 scripts/tests/das2sbs.py DAS.txt das.sbs
mrtk post -k conf/pvs/ppp_southpan_pvs.toml -o rover.pos \
  rover.obs BRD400DLR_S_20251100000_01D_MN.rnx das.sbs
```

`mrtk post` reads SBAS message files by their `.sbs` extension. In the file,
an L5 message is recognised by its type column; L1 messages and the other DFMC
GEOs (for example the BDSBAS GEOs PRN 130/143/144, which also send MT32) are
not used for PVS.

---

## Real-Time

```bash
mrtk run -o conf/pvs/run_southpan_pvs.toml
```

Edit `[streams.input.rover] path` first. A single SBF stream gives the
observations, the PVS messages and the ephemeris; no separate correction
stream is needed. To replay a recorded file in real time, use its plain path
with `::T::x<speed>` and a `.tag` file next to it.

---

## Key Configuration Points

### Correction source

```toml
[positioning]
correction = "southpan-pvs"
satellite_ephemeris = "brdc+ssrapc"
```

`correction = "southpan-pvs"` must be paired with `brdc+ssrapc` (or
`brdc+ssrcom`), and ambiguity resolution must be off.

### Validity

A PVS correction is valid for **100 s from its reception**. Each satellite is
refreshed about every 65 s.

### Clock reference

The corrected GPS clock refers to the L1/L5 iono-free combination: MRTKLIB
removes the broadcast TGD (L1/L2) for PVS. Without it, GPS has a metre-level
bias.

---

## What Accuracy to Expect

Float PPP, last 20 min of one hour against the RINEX header position:

| Case | 2D rms | U rms |
|:--|:-:|:-:|
| ALBY (Albany, AU) 2025-08-21, kinematic | 6.0 cm | 12.6 cm |
| ALBY, static | 8.0 cm | 6.4 cm |

About 12 satellites were used per epoch. One hour is shorter than the IOC
convergence target (≤ 80 min to HPE 0.375 m / VPE 0.525 m, 95 %), and these
numbers are not a service performance figure. A DAS log of ALIC
(2025-04-20) gives about 1 m in MRTKLIB and in cssrlib; the reference position
and the observations are fine (IGS SINEX, CODE-products PPP), so the offset is
in the DAS-logged corrections of that hour (#462).

---

## Limitations

- **Float PPP only.** No phase biases are broadcast.
- **GPS and Galileo only**, on L1 C/A + L5 and E1 + E5a.
- **Forward solutions only** in `mrtk post` (`solution_type = "forward"`): a
  correction is valid for 100 s after its reception.
- **No ionosphere / troposphere corrections** (estimated locally).
- **Regional.** The corrections are for Australia and New Zealand.
- **IOC format.** PVS will move to a new signal (L5b) with a new data format on
  new GEOs; this implementation decodes the IOC MT32.
- **DAS input is not validated** (#462). DAS logs are converted to `.sbs` for
  post-processing; a real-time DAS input (SISNeT stream) is not implemented.

---

## Troubleshooting

### No PVS corrections are applied

- The `.sbs` file has no `122 32` lines: `GEOL5` is not tracked or block 4021
  is not logged.
- `[input.sbas] satellite` is set to another PRN.

### No Galileo satellites in the solution

The navigation file has no Galileo F/NAV records (for example a file with
I/NAV only). Use `BRD400DLR_S` or the navigation file of `mrtk convert`.

---

## Next Steps

- [Quick Start: BeiDou PPP-B2b](quickstart-bds-b2b.md) — another free broadcast float-PPP service
- [Quick Start: Galileo HAS PPP](quickstart-gal-has.md)
- [Configuration Options Reference](../reference/config-options.md) — full options list
