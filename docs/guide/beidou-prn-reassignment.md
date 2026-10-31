# BeiDou PRN Reassignment (April 2026)

Between **2026-04-10 and 2026-04-21** BeiDou retired most BDS-2 satellites and
moved BDS-3 satellites onto the PRNs they freed. Since then, **PRN 1–18 is no
longer a reliable marker of BDS-2**: most of C01–C14 are now BDS-3, and two
BDS-2 satellites are still in service on C09 and C10.

MRTKLIB (like MADOCALIB 2.2) cannot tell the generation of a satellite on PRN
1–18 from the data. You tell it with two configuration keys:

| Key | Section | Meaning | Default |
|:----|:--------|:--------|:--------|
| `bds2_sats` | `[positioning]` | Which of PRN 1–18 are **BDS-2** | none (all BDS-3) |
| `bds3_igso_sats` | `[ambiguity_resolution]` | Which PRNs are **BDS-3 IGSO** (PPP-AR only) | `C06-C08` |

!!! warning "Set `bds2_sats` whenever BeiDou is enabled"
    With the default, every satellite on PRN 1–18 is treated as BDS-3, which
    is wrong for C09/C10 today and for every PRN 1–18 satellite in data taken
    before 2026-04-10. `mrtk post` and `mrtk run` print a warning when BeiDou is
    enabled and `bds2_sats` is absent from the configuration.

---

## Which value to use

| Data taken | `bds2_sats` | `bds3_igso_sats` |
|:-----------|:------------|:-----------------|
| Before 2026-04-10 | `["C01-C18"]` | `["C38-C40"]` |
| After 2026-04-21 (current) | `["C09", "C10"]` | leave unset (`C06-C08`) |
| 2026-04-10 to 2026-04-21 | no single list is right, see [below](#data-from-the-transition-window) | |

```toml
[positioning]
systems = ["GPS", "Galileo", "QZSS", "BeiDou"]
bds2_sats = ["C09", "C10"]       # data after 2026-04-21

[ambiguity_resolution]
# bds3_igso_sats not set: C06-C08
```

```toml
[positioning]
systems = ["GPS", "Galileo", "QZSS", "BeiDou"]
bds2_sats = ["C01-C18"]          # data before 2026-04-10

[ambiguity_resolution]
bds3_igso_sats = ["C38-C40"]
```

Entries are BeiDou satellite IDs (`"C09"`) or ranges (`"C01-C18"`), in any mix.

- **`bds2_sats` only accepts PRN 1–18.** PRN 19 and above are always BDS-3.
- **An empty list `bds2_sats = []` means "no BDS-2".** It is the same as the
  default, but it counts as set, so it silences the warning. Use it only when
  you know the data has no BDS-2 satellite.
- **An empty or absent `bds3_igso_sats` means `C06-C08`.**
- **An invalid list falls back to the default.** The run prints a warning.

---

## Why the generation matters

MRTKLIB keeps BDS-2 and BDS-3 apart wherever they behave differently:

- **Signals and frequencies.** BDS-2 uses B1I / B3I / B2I. BDS-3 uses B1I / B3I
  plus B1C / B2a / B2b, and does not transmit B2I. The `[signals]` `bds2` and
  `bds3` presets select the frequency slots per generation.
- **Receiver clock and biases.** SPP and PPP estimate a separate BDS-2 clock
  offset / inter-system bias.
- **RINEX reading.** Observation codes are mapped to frequency slots per
  generation.
- **PPP-AR.** Ambiguities are differenced within each generation. IGSO and
  BDS-2 satellites are never taken as reference satellites, and are dropped
  first in partial AR.
- **MADOCA-PPP (IS-QZSS-MDC-005).** A BDS-3 satellite on PRN 1–18 may be sent
  in GNSS ID 3, and its signal mask is then read with the BDS-3 signal table.
  A wrong `bds2_sats` gives its code and phase biases the wrong signal codes.

A satellite classified as the wrong generation either loses its observations
(no matching frequency) or contaminates the filter with a wrong signal model.

---

## What changed in April 2026

PRN assignments from the
[IGS satellite metadata](https://files.igs.org/pub/station/general/igs_satellite_metadata.snx)
(see also the IGSMAIL notices, e.g.
[IGSMAIL-8693](https://lists.igs.org/pipermail/igsmail/2026/008689.html)):

Dates are the day the new satellite took the PRN; "was" gives its previous PRN.

| PRN | Before 2026-04-10 | Since | Now |
|:----|:------------------|:------|:----|
| C01 | BDS-2 GEO | 04-20 | BDS-3 GEO (was C59) |
| C02 | BDS-2 GEO | 04-17 | BDS-3 GEO (was C60) |
| C03 | BDS-2 GEO | 04-18 | BDS-3 GEO (was C61) |
| C04 | BDS-2 GEO | 04-21 | BDS-3 GEO (was C62) |
| C05 | BDS-2 GEO | 04-21 | unused |
| C06 | BDS-2 IGSO | 04-10 | **BDS-3 IGSO** (was C38) |
| C07 | BDS-2 IGSO | 04-17 | **BDS-3 IGSO** (was C39) |
| C08 | BDS-2 IGSO | 04-17 | **BDS-3 IGSO** (was C40) |
| C09 | BDS-2 IGSO | 04-18 | **BDS-2 IGSO** (IGSO-6, was C13) |
| C10 | BDS-2 IGSO | 04-18 | **BDS-2 IGSO** (IGSO-7, was C16) |
| C11 | BDS-2 MEO | 04-20 | BDS-3 MEO (was C47) |
| C12 | BDS-2 MEO | 04-21 | BDS-3 MEO (was C48) |
| C13 | BDS-2 IGSO | 04-19 | BDS-3 MEO (was C49) |
| C14 | BDS-2 MEO | 04-19 | BDS-3 MEO (was C50) |
| C16 | BDS-2 IGSO | 04-18 | unused (moved to C10) |
| C38 | BDS-3 IGSO | 04-20 | BDS-3 **MEO** (was C43) |
| C39 | BDS-3 IGSO | 04-19 | BDS-3 **MEO** (was C45) |
| C40 | BDS-3 IGSO | 04-21 | BDS-3 **MEO** (was C46) |

The IGSO change is why `bds3_igso_sats` exists. Before April the BDS-3 IGSO
satellites were C38–C40; today they are C06–C08, and C38–C40 are MEO.

!!! note "Checking your own data"
    B1C (`C1P`/`C1D`/`C1X`) and B2a (`C5P`/`C5D`/`C5X`) are BDS-3-only signals. A
    PRN 1–18 satellite tracked on them is BDS-3; one tracked only on B1I / B3I /
    B2I is BDS-2 (or a receiver that does not track the new signals).

---

## Data from the transition window

Between 2026-04-10 and 2026-04-21 PRNs changed generation one by one, and some
changed in the middle of a day. A single `bds2_sats` list cannot describe such a
day. Two options:

- **Process with the list that matches the end of your session.** Then add the
  PRNs that changed during the session to `[positioning] excluded_sats`.
- **Leave BeiDou out.** Drop it from `systems` for those days.

---

## Bundled configurations

| File | Data | `bds2_sats` | `bds3_igso_sats` |
|:-----|:-----|:------------|:-----------------|
| `conf/madocalib/rnx2rtkp_pppar.toml`, `rnx2rtkp_pppar_iono.toml` | MADOCALIB 2.0 sample (2025) | `C01-C18` | `C38-C40` |
| `conf/madocalib/rnx2rtkp_pppar_mdc005.toml` | MADOCALIB 2.2 sample | `C09, C10` | default |
| `conf/madocalib/rtkrcv_madoca_pppar_iono.toml` | live (`mrtk run`) | `C09, C10` | default |
| `conf/igs/rnx2rtkp_iflc_fallback.toml` | IGS test data, 2026-05-01 | `C09, C10` | default |
| `conf/benchmark/rtk.toml`, `single.toml` | PPC / GSDC (2020–2023) | `C01-C18` | — |

When you adapt a bundled configuration to your own data, check these two keys
against the [table above](#which-value-to-use).

---

## Legacy `.conf` files and MADOCALIB

The keys correspond to MADOCALIB 2.2's options:

| TOML | Legacy `.conf` |
|:-----|:---------------|
| `[positioning] bds2_sats = ["C09", "C10"]` | `pos2-bds2prn = C09,C10` |
| `[ambiguity_resolution] bds3_igso_sats = ["C38-C40"]` | `pos2-bds3igso = C38-C40` |

The defaults are the same as MADOCALIB 2.2: no BDS-2 and IGSO `C06-C08`.
MADOCALIB 2.1 and earlier classified PRN 1–18 as BDS-2 and C38–C40 as IGSO
unconditionally. To reproduce them, use `["C01-C18"]` / `["C38-C40"]`.

The setting is process-wide. `mrtk post` applies it before reading the input
files; `mrtk run` applies it when the server starts.
