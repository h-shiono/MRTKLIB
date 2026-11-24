# Smartphone SPP Benchmark (GSDC-2023)

This benchmark evaluates MRTKLIB's single-point positioning (SPP) on **low-cost
smartphone** GNSS data using the open
[Google Smartphone Decimeter Challenge 2023 (GSDC-2023)][gsdc] dataset.

It is the smartphone counterpart of the geodetic-grade
[PPC kinematic benchmark](benchmark.md).  Smartphone data is where the #116 SPP
accuracy work's last two stages are validated — the geodetic PPC set cannot
exercise them (see [`docs/design/spp-accuracy.md`](../design/spp-accuracy.md)
§4.6):

- **P5 — common-mode clock-jump correction.** Geodetic clocks don't step;
  smartphones do (the GSDC `device_gnss.csv` carries
  `HardwareClockDiscontinuityCount`).
- **P6 — position EKF (smoothing / coasting).** After P1–P4 the geodetic
  per-epoch WLS is already smooth, so there is nothing to smooth; smartphone
  data has large jitter and a real smoothing/coasting payoff.

> **Note:** Like the PPC benchmark, this is intentionally excluded from the
> regular CTest suite — it needs a large external dataset and network access for
> broadcast ephemeris.  Run it on demand.

> **Disclaimer:** The SPP configuration here is **not** tuned for the smartphone
> regime.  These are baseline/reference numbers, not best-achievable accuracy.

---

## Dataset

| | |
|---|---|
| **Source** | Google Smartphone Decimeter Challenge 2023 (Kaggle) |
| **Receivers** | Android phones (Pixel 4/4XL/5/6Pro/7Pro, Xiaomi Mi8, Samsung S20/A32, …) |
| **Rate** | 1 Hz |
| **Reference** | NovAtel SPAN (post-processed), `ground_truth.csv` |
| **Train cases** | 156 `trip/device` combinations with ground truth |
| **Raw format** | `device_gnss.csv` (Google-derived) — **not RINEX** |

The raw measurements are *not* RINEX.  GSDC ships a **derived** CSV in which
Google has already reconstructed the pseudorange, classified the signal and
provided carrier phase / Doppler / C/N0, so a direct CSV → RINEX mapping
(`gsdc_to_rinex.py`) is simpler and less error-prone than re-deriving the
pseudorange from the raw Android clock fields.

### Manual download

The archive (~3.5 GB zip → ~16 GB extracted) must be downloaded manually from
Kaggle (account + competition rules acceptance required):

1. Visit the [GSDC-2023 data page][gsdc] and download the competition data
   (or use the Kaggle CLI: `kaggle competitions download -c smartphone-decimeter-2023`).
2. Place the zip at `tests/data/sdc2023.zip` (git-ignored), or extract it so the
   layout matches:

```
data/gsdc/
  train/
    2020-06-25-00-34-us-ca-mtv-sb-101/
      pixel4/
        device_gnss.csv      # raw (derived) measurements
        ground_truth.csv     # reference track (lat/lon/alt + speed/bearing)
    ...
  brdc/                      # broadcast nav cache (auto-downloaded)
  results/                  # OBS + NMEA output
```

Only `device_gnss.csv` and `ground_truth.csv` are needed per case; the
`device_imu.csv` and `supplemental/` files are not used.  Both `data/gsdc/` and
`tests/data/sdc2023.zip` are git-ignored.

Extract just the curated subset (≈250 MB) without unpacking the whole archive:

```bash
for tp in \
  train/2020-06-25-00-34-us-ca-mtv-sb-101/pixel4 \
  train/2020-07-17-23-13-us-ca-sf-mtv-280/pixel4 \
  train/2020-12-10-22-52-us-ca-sjc-c/mi8 \
  train/2021-04-02-20-43-us-ca-mtv-f/sm-g988b \
  train/2021-12-07-19-22-us-ca-lax-d/pixel5 \
  train/2023-09-05-23-07-us-ca-routen/pixel7pro ; do
  unzip -o -q tests/data/sdc2023.zip "$tp/device_gnss.csv" "$tp/ground_truth.csv" -d data/gsdc/
done
```

---

## Conversion: `device_gnss.csv` → RINEX 3.04

`gsdc_to_rinex.py` maps each measurement row to a RINEX 3.04 observation:

| RINEX | Source column | Notes |
|-------|---------------|-------|
| C (pseudorange) | `RawPseudorangeMeters` | already reconstructed by Google |
| L (carrier) | `AccumulatedDeltaRangeMeters` / λ | only when ADR state = VALID; LLI set on RESET/CYCLE_SLIP |
| D (Doppler) | −`PseudorangeRateMetersPerSecond` / λ | |
| S (C/N0) | `Cn0DbHz` | full dB-Hz in the S obs (e.g. `S1C` = `40.200`); a 1–9 SSI digit is also appended to every field per RINEX |

with λ = c / `CarrierFrequencyHz`.  Satellite id is `(ConstellationType, Svid)`
→ `G/R/E/C/J` (QZSS PRN = Svid − 192).  Epoch GPS time = `utcTimeMillis`/1000
− 315964800 + `LeapSecond` (cross-checked against the CSV's own
`ArrivalTimeNanosSinceGpsEpoch`).

Signal → RINEX 3.04 observation code:

| Android `SignalType` | RINEX code | | Android `SignalType` | RINEX code |
|----------------------|:----------:|---|----------------------|:----------:|
| `GPS_L1_CA` | `1C` | | `GAL_E5A_Q` | `5Q` |
| `GPS_L5_Q` | `5Q` | | `GLO_G1_CA` | `1C` |
| `GAL_E1_C_P` | `1C` | | `QZS_L1_CA` | `1C` |
| `BDS_B1_I` | `2I` | | `QZS_L5_Q` | `5Q` |

GLONASS is converted but excluded from positioning by the SPP config's
`systems` list (FDMA, no broadcast iono model fit for single-frequency SPP).

**Converter self-check.** `--self-check` reproduces Google's per-epoch WLS from
the CSV's own `SvPosition*`/clock/iono/tropo columns and compares it to the
`WlsPosition*` column.  Agreement to a few metres confirms the
pseudorange / satellite-id / epoch-grouping reading (a reading bug diverges by
orders of magnitude, not metres):

```bash
python scripts/benchmark/gsdc_to_rinex.py \
    data/gsdc/train/.../pixel4/device_gnss.csv --self-check
```

---

## Broadcast ephemeris

`device_gnss.csv` carries no ephemeris, so `download_brdc.py` fetches the daily
merged multi-GNSS broadcast nav (RINEX 3, GPS+GLO+GAL+BDS+QZS) for each UTC day
a trip spans.  The source is **BKG** (`BRDM00DLR_S`, falling back to
`BRDC00WRD_S` / `BRDC00IGS_R`), which serves without authentication (CDDIS
requires an Earthdata login).  Files are cached under `data/gsdc/brdc/`.

---

## Quick start

```bash
# 1. Build mrtk and extract the test ATX/ISB/ERP data (as for any benchmark).
cmake --build build
cd build && ctest -R setup --output-on-failure && cd ..

# 2. Run the curated subset (downloads broadcast nav automatically).
python scripts/benchmark/run_gsdc_benchmark.py --case curated

# 3. Single case, or every discovered train case:
python scripts/benchmark/run_gsdc_benchmark.py \
    --case 2021-12-07-19-22-us-ca-lax-d/pixel5
python scripts/benchmark/run_gsdc_benchmark.py --case all
```

| Option | Default | Description |
|--------|---------|-------------|
| `--dataset-dir DIR` | `data/gsdc` | GSDC root (contains `train/`) |
| `--brdc-dir DIR` | `data/gsdc/brdc` | broadcast-nav cache |
| `--out-dir DIR` | `data/gsdc/results` | OBS + NMEA output |
| `--case curated\|all\|<id,…>` | `curated` | which cases to run |
| `--conf PATH` (repeatable) | `conf/benchmark/single.toml` | layered config |
| `--skip-download` | off | use cached nav only |
| `--threshold M` | `2.0` | 2D `<N m` rate threshold |
| `--force` | off | re-convert even if cached |

---

## Scoring (official GSDC metric)

The Google Smartphone Decimeter Challenge scores each run by the **mean of the
50th and 95th percentile horizontal positioning error** (metres); the per-run
scores are averaged to give the leaderboard number.  The official competition
computes this over the **36 test runs** (public/private split), whose ground
truth is withheld.

This benchmark reports the same metric on the **train** split, which has public
ground truth.  The numbers are on the leaderboard's scale but are not
leaderboard (test split) results.  Top GSDC entries reach ~1–2 m using
carrier-phase PPP/RTK + IMU + map-matching; the SPP-only figures here are a
code-pseudorange baseline, not a leaderboard-competitive solution.

The competition scored a position at **every ground-truth epoch**, so an epoch
without a solution cannot be left out.  The runner prints two scores:

| Column | Epochs | Gaps |
|--------|--------|------|
| `official` | every ground-truth epoch, horizontal (latitude / longitude) error only | each epoch takes the latest solution at or before it (within 0.15 s it is the epoch's own solution); epochs before the first solution take the first solution |
| `matched` | only epochs with a solution within 0.15 s | left out |

Scoring every truth epoch is the competition's rule; the fill rule is this
benchmark's choice.  `fill%` is the share of a case's epochs scored with a held
or back-filled value (the `MEAN` row averages it per case).  The competition
left gap filling to the entrant (interpolation was common); holding
the latest solution is what a real-time user would see and does not use future
solutions.  `matched` is kept for engineering insight but overstates accuracy
when solutions are missing, so compare configurations on `official`.  The
`MEAN` of `official` is over every case with ground truth; if any case cannot
be scored (conversion, navigation or solver failure, or no solution at all) it
is shown as `incomplete`.  `--skip-epochs` does not apply to `official`.

The runner also prints `<2 m` rate, RMS and matched-epoch count `N`
(availability).  Per-epoch ENU error is taken against the ground-truth
coordinate (moving-base projection), as in the
[PPC benchmark](benchmark.md#metrics-definitions); for `matched`, reference and
NMEA GGA epochs are matched by UTC seconds-of-day within 0.15 s.

---

## Baseline results (curated subset)

GNSS-only SPP, broadcast ephemeris, no skip.  The `matched` columns were first
measured on v0.6.10 and are unchanged on develop 9b3a220, where the `official`
and `fill%` columns were added.  Two
configurations bracket the #116 work and form the before/after reference for the
P5/P6 effort:

- **P0** — classic SPP: elevation-only weighting, Klobuchar iono, Saastamoinen
  tropo, RAIM-FDE (`single.toml` + `gsdc_p0.toml`).
- **P1–P4** — `single.toml` as shipped: + C/N0 weighting, IGG-III robust +
  pre-robust gate, TDCP jump rejection.

`official` is the GSDC metric over every ground-truth epoch; lower is better.

### P0 (classic SPP)

| Case | N | nSV | <2 m | RMS 2D | p50 | p95 | matched | **official** | fill% |
|------|--:|----:|-----:|-------:|----:|----:|--------:|-------------:|------:|
| mtv-sb-101 / pixel4 | 1289 | 14.3 | 21.5% | 4.30 m | 3.34 m | 7.61 m | 5.47 m | 5.65 m | 0.8% |
| sf-mtv-280 / pixel4 | 1680 | 11.6 | 20.7% | 4.98 m | 3.68 m | 8.94 m | 6.31 m | 6.42 m | 1.1% |
| sjc-c / mi8 | 1266 | 15.0 | 24.7% | 4.71 m | 3.16 m | 8.72 m | 5.94 m | 6.57 m | 10.1% |
| mtv-f / sm-g988b | 2284 | 19.9 | 37.0% | 3.35 m | 2.58 m | 6.03 m | 4.30 m | 4.30 m | 0.0% |
| lax-d / pixel5 | 1716 | 12.4 | 22.1% | 4.30 m | 3.37 m | 7.56 m | 5.47 m | 5.62 m | 2.4% |
| routen / pixel7pro | 1545 | 14.7 | 22.7% | 4.58 m | 3.41 m | 8.68 m | 6.05 m | 6.15 m | 1.2% |
| **mean** | **1630** | **14.6** | **24.8%** | **4.37 m** | **3.26 m** | **7.92 m** | 5.59 m | **5.79 m** | 2.6% |

### P1–P4 (`single.toml`)

| Case | N | nSV | <2 m | RMS 2D | p50 | p95 | matched | **official** | fill% |
|------|--:|----:|-----:|-------:|----:|----:|--------:|-------------:|------:|
| mtv-sb-101 / pixel4 | 685 | 14.4 | 36.8% | 3.00 m | 2.41 m | 5.45 m | 3.93 m | 55.56 m | 47.3% |
| sf-mtv-280 / pixel4 | 370 | 11.8 | 34.3% | 3.12 m | 2.63 m | 5.47 m | 4.05 m | 234.77 m | 78.2% |
| sjc-c / mi8 | 815 | 15.1 | 36.0% | 3.62 m | 2.51 m | 6.88 m | 4.69 m | 17.72 m | 42.1% |
| mtv-f / sm-g988b | 2072 | 19.9 | 42.2% | 2.93 m | 2.33 m | 5.26 m | 3.80 m | 9.59 m | 9.3% |
| lax-d / pixel5 | 735 | 12.5 | 37.6% | 3.13 m | 2.42 m | 5.25 m | 3.84 m | 65.11 m | 58.2% |
| routen / pixel7pro | 768 | 14.8 | 28.6% | 3.86 m | 3.03 m | 6.64 m | 4.84 m | 24.39 m | 50.9% |
| **mean** | **907** | **14.7** | **35.9%** | **3.28 m** | **2.56 m** | **5.82 m** | 4.19 m | **67.86 m** | 47.7% |

### Reading the two tables

On the matched epochs alone, P1–P4 lowers the score from 5.59 m to 4.19 m
(−25 %; p50 3.26 → 2.56 m, p95 7.92 → 5.82 m, `<2 m` +11 pp).  It gets there
by rejecting epochs: the matched-epoch count drops ~44 % (mean 1630 → 907),
and on the official metric, which scores every epoch, P1–P4 is worse than P0
(67.86 m vs 5.79 m).  The gaps are long enough that a held position drifts far
from a moving phone (sf-mtv-280: 78 % of epochs filled, 234.77 m).  The
conclusion does not depend on the fill rule: with linear interpolation between
solutions instead of holding, the means are 7.51 m (P1–P4) and 5.69 m (P0).

The remaining work therefore has to recover availability, not only accuracy:

- **P6 (position EKF)** should *coast* through the rejected epochs, recovering
  availability without giving back accuracy — and smartphone jitter (3–4 m, vs
  the geodetic set's sub-decimetre) is where smoothing actually pays
  ([`spp-accuracy.md`](../design/spp-accuracy.md) §4.6).
- **P5 (clock-jump correction)** should recover epochs lost to receiver clock
  steps and stabilise TDCP on low-cost hardware.

Google's own WLS (the `WlsPosition*` column) lands at ~2.3 m 2D RMS on the open
trips, so MRTKLIB's untuned SPP is already in the same regime; the headroom is
in availability and the urban tail.

---

## Known limitations

- **Untuned SPP config.** `single.toml` was tuned on the geodetic PPC set; the
  smartphone regime (clock jumps, larger noise, NLOS) is not yet tuned — that is
  the P5/P6 work.
- **No antenna / ISB calibration.** Phone antennas have no PCV; Google's
  satellite ISB is not applied (the converter keeps `RawPseudorangeMeters`).
- **GLONASS excluded** from positioning (converted but dropped by the config).
- **Train split only.** The `test/` split has no public ground truth.
- **IMU not used.** GNSS-only.

---

## Acknowledgements

The dataset is the **Google Smartphone Decimeter Challenge 2023**, hosted on
Kaggle and released by Google's Android GNSS team in collaboration with the
ION GNSS+ conference.  Please observe the Kaggle competition rules and cite the
GSDC-2023 materials when publishing results derived from this data.

---

## References

- [GSDC-2023 on Kaggle][gsdc]
- [SPP accuracy design](../design/spp-accuracy.md) (#116 P0–P6)
- [PPC kinematic benchmark](benchmark.md)

[gsdc]: https://www.kaggle.com/competitions/smartphone-decimeter-2023
