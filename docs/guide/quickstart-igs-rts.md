# Quick Start: IGS-RTS PPP

Conventional **float PPP** from the **IGS Real-Time Service (RTS)** — a global,
open, real-time **RTCM-SSR / IGS-SSR (MT4076)** correction stream (orbit, clock,
code bias). No base station and no regional augmentation are required; corrections
arrive over the internet via NTRIP.

Set `correction = "igs-rts"` and feed an RTCM-SSR stream alongside your rover
observations and a broadcast navigation source. Typical accuracy is **~0.2 m
horizontal (float)** after convergence — see [Expected accuracy](#expected-accuracy).

!!! info "igs-rts vs igs"
    `correction = "igs"` is *post-processing* with precise SP3/CLK **files**.
    `correction = "igs-rts"` is *real-time* SSR over NTRIP. Both run the same
    RTKLIB-style float-PPP measurement model; they differ only in the
    orbit/clock source. See the
    [Positioning Configuration Model](../design/configuration.md).

---

## Prerequisites

1. **An NTRIP account for an RTS caster.** This guide uses NASA **CDDIS**, which
   requires a free **Earthdata** login.
    - Register: [https://urs.earthdata.nasa.gov/](https://urs.earthdata.nasa.gov/)
    - CDDIS real-time / NTRIP overview:
      [Earthdata — GNSS real-time data](https://www.earthdata.nasa.gov/data/space-geodesy-techniques/gnss/real-time-data)
    - CDDIS caster stream table (mountpoints):
      [CDDIS — Data caster streams](https://cddis.nasa.gov/Data_and_Derived_Products/Data_caster_streams.html)
2. **stunnel** — CDDIS serves NTRIP over TLS (port 443). MRTKLIB has no built-in
   TLS stack; tunnel through stunnel. See [NTRIP Streams → TLS](ntrip.md#tls-connections-ntrip-2s)
   for the full setup. The minimal tunnel config is shown below.
3. **A dual-frequency rover.** Geodetic receivers (GPS L1/L2, GAL E1/E5a) are
   ideal. Other RTS casters (BKG `products.igs-ip.net`, etc.) work the same way.

---

## Streams and mountpoints

An IGS-RTS PPP run needs three data sources. On CDDIS:

| Role | What | Example mountpoint |
|------|------|--------------------|
| **SSR correction** | orbit/clock/code-bias (RTCM-SSR / IGS-SSR) | `SSRA02IGS0` (IGS combined, APC, ITRF) |
| **Broadcast ephemeris** | RTCM-3 nav (1019/1020/1042/1044/1045/1046) | `BCEP00BKG0` |
| **Rover observations** | your receiver, or an IGS station for testing | e.g. `AIRA00JPN` (Aira, Japan) |

Notes:

- **SSR provider variants.** `SSRAxxIGS0` is the IGS combined product in the
  global **ITRF** frame; provider-specific streams exist too (`SSRA00CNE0` CNES,
  `SSRA00WHU0` WHU, …). Prefer the **ITRF** variant — the `_SIRGAS2000` variants
  are re-projected for South America and add a frame bias elsewhere. CNES streams
  additionally carry **phase bias** (future PPP-AR); IGS combined is code-bias
  only (float).
- **`SSRA` = antenna-phase-center (APC)** referenced → use
  `satellite_ephemeris = "brdc+ssrapc"`. `SSRC` = center-of-mass → `brdc+ssrcom`.
- **Broadcast ephemeris is required** — SSR corrects the broadcast orbit/clock,
  it does not replace it. Many receiver streams already embed RTCM nav (1019…);
  if yours does not, add a `BCEP*` stream (or a RINEX nav file for `mrtk post`).
- Pick mountpoints from the [CDDIS stream table](https://cddis.nasa.gov/Data_and_Derived_Products/Data_caster_streams.html).

---

## Connect (stunnel TLS tunnel)

`ntrip-tls.conf`:

```conf
[ntrip-tls]
client = yes
accept = 127.0.0.1:2101
connect = caster.cddis.eosdis.nasa.gov:443
```

```bash
stunnel ntrip-tls.conf      # listens on 127.0.0.1:2101, forwards to CDDIS over TLS
```

Then connect MRTKLIB through the local endpoint with NTRIP v2 and the `&host=`
header (required by most TLS casters). URL-encode special characters in the
password (`@` → `%40`). Full details in [NTRIP Streams](ntrip.md).

---

## Configuration

Sample: [`conf/igs/rnx2rtkp_igsrts.toml`](https://github.com/h-shiono/MRTKLIB/blob/main/conf/igs/rnx2rtkp_igsrts.toml).

```toml
[positioning]
mode = "ppp-static"                  # or ppp-kine for a moving rover
frequency = "l1+2"
satellite_ephemeris = "brdc+ssrapc"  # SSRA = APC; use brdc+ssrcom for SSRC
systems = ["GPS", "Galileo"]
correction = "igs-rts"               # must be set explicitly (see note)

[positioning.corrections]
satellite_antenna = false            # SSR_APC orbits are antenna-phase-center based
receiver_antenna = true

[positioning.atmosphere]
ionosphere = "dual-freq"             # iono-free combination
troposphere = "est-ztdgrad"
```

!!! warning "`igs-rts` is never auto-inferred"
    `igs-rts` shares `satellite_ephemeris = "brdc+ssrapc"` with `qzs-madoca`, so
    omitting `correction` infers **`qzs-madoca`**, not `igs-rts`. You must set
    `correction = "igs-rts"` explicitly. It requires `brdc+ssrapc` (or
    `brdc+ssrcom`) and is rejected at load time otherwise.

!!! note "Adding BeiDou"
    If you add BeiDou to `systems`, also set `[positioning] bds2_sats = ["C09", "C10"]`
    (the BDS-2 satellites left on PRN 1–18 since April 2026). See
    [BeiDou PRN Reassignment](beidou-prn-reassignment.md).

---

## Running

### Post-processing (`mrtk post`)

Record the streams to RTCM-3 logs, then process. Capture with `mrtk relay`
(stagger the starts a few seconds apart to avoid simultaneous-connect rejections
on CDDIS):

```bash
Q='ver=2&host=caster.cddis.eosdis.nasa.gov'
mrtk relay -in "ntrip://USER:PW@127.0.0.1:2101/SSRA02IGS0?$Q"  -out ssr.rtcm3  &
mrtk relay -in "ntrip://USER:PW@127.0.0.1:2101/BCEP00BKG0?$Q"  -out brdc.rtcm3 &
mrtk relay -in "ntrip://USER:PW@127.0.0.1:2101/AIRA00JPN?$Q"   -out rover.rtcm3 &
```

Convert obs/nav to RINEX, then run PPP (the SSR `.rtcm3` log is auto-detected by
its extension):

```bash
mrtk convert -r rtcm3 -o rover.obs -tr 2026/05/22 14:00:00 rover.rtcm3
mrtk convert -r rtcm3 -n brdc.nav  -tr 2026/05/22 14:00:00 brdc.rtcm3
mrtk post -k conf/igs/rnx2rtkp_igsrts.toml rover.obs brdc.nav ssr.rtcm3 -o out.pos
```

!!! tip "`-tr` takes two tokens"
    `mrtk convert -tr` expects the approximate log start as **two** arguments
    (`Y/M/D` and `H:M:S`), e.g. `-tr 2026/05/22 14:00:00` — not one quoted string.

### Real-time (`mrtk run`)

Use the real-time sample
[`conf/igs/rtkrcv_igsrts.toml`](https://github.com/h-shiono/MRTKLIB/blob/main/conf/igs/rtkrcv_igsrts.toml):
it points `[streams.input.rover]` and `[streams.input.correction]` at NTRIP
mountpoints (type `ntripcli`) through the stunnel endpoint, with the same
`[positioning]` block as above. Replace `USER:PW` and the rover stream with your
own receiver, then start:

```bash
mrtk run -o conf/igs/rtkrcv_igsrts.toml -p 2401 -s
```

The correction-application pipeline (`decode_ssr*` → `satpos_ssr` → `corr_meas`)
is identical to the post-processing path; both reach the same accuracy level.

---

## Expected accuracy

Validated on a real CDDIS RTS stream — IGS station **AIRA00JPN** (GPS L1/L2 +
GAL E1/E5a iono-free, TRM59800.00 SCIS), IGS combined RTCM-SSR (`SSRA02IGS0`) +
broadcast ephemeris, against the IGS final SINEX coordinate:

| Path | 3D 1σ | 3D 95% |
|------|-------|--------|
| Post-processing (`mrtk post`) | ~0.18 m | ~0.33 m |
| Real-time (`mrtk run`, replay) | ~0.27 m | ~0.38 m |

This is conventional **float** PPP (code-bias-only product). The result agrees
with upstream RTKLIB 2.4.3 `rnx2rtkp` on identical inputs. Integer PPP-AR (a
phase-bias product such as CNES `CLK9x`) is a future addition.

!!! note "Verify against a SINEX, not a webpage coordinate"
    When checking absolute accuracy, compare against an **IGS SINEX**
    coordinate propagated to the observation epoch
    (`scripts/tests/compare_pos_abs.py --sinex FILE --station CODE --epoch …`).
    A station-page LLH is an ITRF reference-epoch value and can be off by tens of
    centimetres of plate motion in tectonically active regions (e.g. Japan).

---

## Troubleshooting

- **`correction source not implemented` / `invalid correction`** — set
  `satellite_ephemeris = "brdc+ssrapc"` (or `"brdc+ssrcom"`); `igs-rts` requires
  it.
- **HTTP 401 / no data on capture** — check the Earthdata credentials and the
  `&host=` header; when capturing several streams at once, start them a few
  seconds apart (CDDIS rejects bursts of simultaneous connects).
- **No solution (`Q=0`) / metre-level bias** — confirm the broadcast ephemeris
  covers the obs window and that the SSR is the **ITRF** (not `_SIRGAS2000`)
  variant. A real-time MSM RINEX often carries **no antenna name**; set
  `[antenna.rover] type` explicitly so the receiver PCV is applied.
- **`mrtk run`: `console open error dev=`** — pass a telnet port (`-p 2401`); the
  follow-on `Error: vt is NULL` is harmless (no attached terminal).

---

## Next steps

- [NTRIP Streams](ntrip.md) — NTRIP versions, TLS/stunnel, password escaping.
- [Configuration (TOML)](configuration.md) and
  [Config Options Reference](../reference/config-options.md).
- [Positioning Configuration Model](../design/configuration.md) — the
  `correction` axis and validity matrix.
