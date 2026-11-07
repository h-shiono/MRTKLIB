# MRTKLIB : Modernized RTKLIB for Next-Generation GNSS

[![License](https://img.shields.io/badge/License-BSD_2--Clause-blue.svg)](LICENSE)
[![Build](https://github.com/h-shiono/MRTKLIB/actions/workflows/build.yaml/badge.svg?branch=main)](https://github.com/h-shiono/MRTKLIB/actions/workflows/build.yaml)
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)]()
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.20373746.svg)](https://doi.org/10.5281/zenodo.20373746)

**MRTKLIB** is a thread-safe C11 GNSS positioning library built on [RTKLIB](https://www.rtklib.com/). It processes the **open PPP / PPP-RTK correction services broadcast by satellites** — QZSS CLAS and MADOCA-PPP, Galileo HAS, BeiDou PPP-B2b and SouthPAN PVS — as well as IGS products and real-time streams, in one code base, for both post-processing (`mrtk post`) and real-time processing (`mrtk run`).

In the spirit of RTKLIB, it aims to be both a readable reference for how each service is applied and practical software for day-to-day and production use. The CLAS and MADOCA-PPP engines are regression-tested for parity with the official [CLASLIB](https://github.com/QZSS-Strategy-Office/claslib) / [MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib), and absolute accuracy is checked against independent truth (e.g. IGS SINEX). Standard positioning (SPP, RTK) is maintained alongside, with selected algorithm improvements from [demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB).

To our knowledge, MRTKLIB is the first open-source positioning engine that runs all five of these services in real time, from the correction messages output by the receiver to the position solution, in one code base. [CSSRlib](https://github.com/hirokawa/cssrlib) provides Python processing examples for the same services; open-source tools for single services include [CLASLIB](https://github.com/QZSS-Strategy-Office/claslib) and [MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib), [HASlib](https://github.com/nlsfi/HASlib) and [HASPPP](https://github.com/ZhangRunzhi20/HASPPP) (Galileo HAS), and [B2bLIB](https://github.com/GCCLib/B2bLIB) and [RTKLIB-B2b](https://github.com/UCAS-Liuchunbo/RTKLIB-B2b) (PPP-B2b).

---

## Changes from RTKLIB

MRTKLIB keeps the RTKLIB algorithms and data structures where they work, and changes the parts that limited reuse:

* **Per-instance state:** a context structure (`mrtk_ctx_t`) holds state, error handling and logging for each instance, in place of global variables in the main processing paths, so that several solvers can run in one process (for example, several streams in one server).
* **POSIX / C11:** the library does not use the Win32 API. CI builds it and runs the regression tests, without the real-time replays, on Linux (Ubuntu, x86-64); the full suite, including the replays, is run on macOS (Apple Silicon).
* **CMake build:** a single CMake build replaces the per-platform Makefiles. LAPACK is located with `find_package(LAPACK)` instead of the bundled Intel MKL binaries; without it, the internal LU solver is used.
* **Source layout by function:** the flat source directory is split into `src/core/`, `src/pos/`, `src/rtcm/`, `src/models/` and others.

## Supported Correction Services

| Service | Delivery | Positioning modes | Post (`mrtk post`) | Real-time (`mrtk run`) |
|---------|----------|-------------------|:-:|:-:|
| **QZSS CLAS** | L6D (CSSR) | PPP-RTK; VRS-RTK (`mrtk cssr2rtcm3` for a real-time RTCM3 VRS stream) | ✅ 1ch / 2ch | ✅ 1ch / 2ch |
| **QZSS MADOCA-PPP** | L6E (SSR) + L6D wide-area ionosphere (PRN 200/201) | PPP, PPP-AR, PPP-AR + iono | ✅ | ✅ |
| **Galileo HAS** | E6-B C/NAV | Float PPP | ✅ | ✅ |
| **IGS products** | SP3 / CLK / Bias-SINEX files | Float PPP, integer PPP-AR (OSB phase biases) | ✅ | — |
| **IGS-RTS** | RTCM-SSR over NTRIP | Float PPP | ✅ (recorded stream) | ✅ |
| **BeiDou PPP-B2b** | BDS-3 GEO B2b-I | Float PPP (BeiDou) | ✅ | ✅ |
| **SouthPAN PVS** | SBAS L5 MT32 (PRN 122) | Float PPP (GPS L1/L5, Galileo E1/E5a) | ✅ | ✅ |

For each service's constellations, receiver inputs, release history and regression tests, see the [capability matrix](docs/reference/correction-services.md).

The service is selected with the `correction` configuration axis; see the [configuration guide](docs/guide/configuration.md) and the per-service quick starts under [`docs/guide/`](docs/guide/). The CLAS engine also handles single-band configurations (single-frequency signal sets and the Galileo E1/E5b mismatch of u-blox ZED-F9P-class receivers); see [`docs/design/sf-ppp-rtk.md`](docs/design/sf-ppp-rtk.md).

Standard positioning — SPP (C/N0 weighting, IGG-III robust estimation, TDCP) and RTK (demo5 back-ports) — is supported in both post-processing and real time.

### Lineage

MRTKLIB began by unifying the QZSS augmentation libraries into one code base on a modernized RTKLIB core; that integration is complete:

| Component | Version | Description | Status |
|-----------|---------|-------------|--------|
| **[MALIB](https://github.com/JAXA-SNU/MALIB)** | feature/1.2.0 (`f006a34`) | MADOCA-PPP structural base (directory layout, threading, streams), by JAXA and TOSHIBA | Integrated |
| **[MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib)** | ver.2.0 (`8091004`) | PPP/PPP-AR engine, L6E SSR decoder, L6D ionospheric decoder | Integrated |
| **[CLASLIB](https://github.com/QZSS-Strategy-Office/claslib)** | ver.0.8.2 (`9e714b9`) | Centimeter Level Augmentation Service (PPP-RTK, VRS-RTK, CSSR decoder) | Integrated |

Real-time CLAS PPP-RTK (`mrtk run`) has been available since v0.4.3 (March 2026); the release in which each service was added is listed in the [capability matrix](docs/reference/correction-services.md#release-history).

### Algorithm Improvements

MRTKLIB's positioning engines have been progressively modernized across releases
(community-fork back-ports, new correction sources, tooling). Per-version detail
is in the [CHANGELOG](CHANGELOG.md) and [release notes](docs/releases/):

| Era | Theme |
|-----|-------|
| **v0.4.x** | demo5 RTK / PPP-RTK algorithm port (PAR, `detslp_dop` / `detslp_code`, full-constellation `varerr`, false-fix fixes); real-time CLAS PPP-RTK (1ch / 2ch) |
| **v0.5.x** | TOML configuration; code-quality sweeps (`clang-format`, mandatory braces); signals restructuring; RINEX 4.00 CNAV; `convbin` / `str2str` ports |
| **v0.6.x** | Unified `mrtk` CLI; NTRIP v2; `mrtk cssr2rtcm3` (CSSR→RTCM3); IGS-products float / RTS / integer PPP-AR (the `correction` axis); SPP accuracy; formatter CI gate; GSDC smartphone benchmark; real-time MADOCA-PPP multi-GNSS signal selection |
| **v0.7.x** | Global SSR correction services: Galileo HAS (E6-B C/NAV) float PPP; two-receiver QZSS L6 (mosaic-CLAS / mosaic-G5) with single-SBF MADOCA-PPP; CLAS PPP-RTK accuracy/robustness fixes and an upstream-identical CSSR dump (`mrtk dump --parse`); real-time MADOCA-PPP PPP-AR/IONO (L6D wide-area ionosphere); CLAS PPP-RTK claslib parity closing the storm-day fix-rate gap; TOML configuration redesign with deprecation aliases; CLAS dual-channel transmit-pattern merge in post-processing and real-time; kinematic-RTK slip-detection regression fix; `rtcm_t` memory-footprint refactor (lazy `lclblock_t`: ~1.2 GB → tens of MB per real-time solver); RTCM3 MSM BDS-3 B2b observation support (decoder + encoder); build-independent MADOCA regression tests with absolute checks; QZSS L6 service routing by frame header with QZSS L6 handover regression tests; MADOCALIB 2.2 sync with configurable BDS-2 / BDS-3 classification for the April 2026 BeiDou PRN reassignment; real-time CLAS robustness to L6 frame losses, phase-bias discontinuities and degraded L6 channels, in the two-channel merge and in `mrtk cssr2rtcm3`; BeiDou PPP-B2b float PPP (post and real time); SouthPAN PVS float PPP (post and real time) |

**Latest — v0.7.16:** **BeiDou PPP-B2b.** `correction = "bds-b2b"` runs BeiDou-only float PPP with the PPP-B2b corrections of the four BDS-3 GEOs, in `mrtk post` and `mrtk run` ([#417](https://github.com/h-shiono/MRTKLIB/issues/417)). The GEOs moved to C01–C04 with the April 2026 PRN reassignment (C59–C62 before) and are configured by the new `[positioning] bds_geo_sats`. In real time the B-CNAV1 ephemeris the corrections refer to comes from SBF `BDSCNav1` (4251), and the decoder takes one GEO stream at a time, because C01–C03 and C04 send different IOD SSR. On a 2 h live capture in Tokyo after 60 min: static 1.8 cm 2D / 3.9 cm U rms, kinematic 15.8 / 10.8 cm, real-time replay 14.8 / 10.7 cm. Two contributed fixes: the single-frequency NavIC L5 group delay in SPP ([#384](https://github.com/h-shiono/MRTKLIB/pull/384)) and the NTRIP v2 source handshake, which sent a raw stream after advertising chunked transfer ([#385](https://github.com/h-shiono/MRTKLIB/pull/385)). New dataset `g5p3_296u-v1`. ABI changes in `prcopt_t`, `raw_t`, `rtksvr_t`. See the [Quick Start: BeiDou PPP-B2b](docs/guide/quickstart-bds-b2b.md) and the [release notes](docs/releases/release-notes-v0.7.16.md).

**v0.7.15:** **CLAS real-time robustness, part 2: phase-bias discontinuities, L6 channel management and partial-AR integrity.** PPP-RTK now honours the CSSR phase discontinuity indicator: when CLAS steps the phase biases, the phase-bias states of the affected signals are re-initialized instead of every phase residual being rejected, which was the leading cause of Single epochs in 5.9 days of live data ([#393](https://github.com/h-shiono/MRTKLIB/issues/393)). With `l6_merge`, a channel resuming across a GPS week rollover no longer stops the corrections until restart (fix rate 38.85 % → 98.95 % on the 5.9-day replay, [#436](https://github.com/h-shiono/MRTKLIB/issues/436)), and a channel whose PRN has gone silent no longer counts as a correction source ([#443](https://github.com/h-shiono/MRTKLIB/issues/443)). A single-channel L6 lock moves from a degraded PRN to a clean one ([#414](https://github.com/h-shiono/MRTKLIB/issues/414)). Partial AR restores a satellite as it was before a trial exclusion ([#413](https://github.com/h-shiono/MRTKLIB/issues/413)) and VRS-RTK partial AR no longer writes before `rtk->ssat[]` ([#412](https://github.com/h-shiono/MRTKLIB/issues/412)). MADOCA-PPP L6E / L6D-iono messages are no longer decoded across a lost L6 frame ([#363](https://github.com/h-shiono/MRTKLIB/issues/363)). `mrtk cssr2rtcm3` bridges a lost correction set with the last good one and outputs the latest receiver epoch instead of stepping behind after input gaps; **it now takes SBF input only (`-in sbf://…`; raw L6 input, `-2ch` and `-pos` are removed)** ([#442](https://github.com/h-shiono/MRTKLIB/issues/442)). The NMEA GGA station id names the CLAS L6 source of the solution. claslib post-processing and VRS outputs are byte-identical or within 0.1 mm. Several CLAS / MADOCA structures gain fields (ABI change). RT replay tests run concurrently: the full suite takes ~8 min instead of ~37 min on 8 cores ([#448](https://github.com/h-shiono/MRTKLIB/issues/448)). The regression suite is 196 / 196.

**v0.7.14:** **CLAS real-time robustness: L6 frame losses and the two-channel merge.** Four real-time CLAS PPP-RTK faults found on live mosaic-G5 data are fixed. Losing subframe 1 of a CLAS frame no longer costs ~17 s of Single: the code bias of the next frame is used, as the consistency check allows ([#406](https://github.com/h-shiono/MRTKLIB/issues/406)). An L6 frame lost inside a subframe (a 1–2 s input loss or an RS failure) is detected from the receiver frame time (SBF / UBX), and the rest of the subframe is discarded instead of being decoded from shifted bits; those garbage corrections drove metre-level Float, false fixes and the reset loop of [#381](https://github.com/h-shiono/MRTKLIB/issues/381) ([#363](https://github.com/h-shiono/MRTKLIB/issues/363)). For this the ST1 mask width check was fixed: it counted from the wrong bits, as upstream claslib has since 0.8.0. Losing the top-of-hour ST1 no longer dates the following messages one hour old ([#392](https://github.com/h-shiono/MRTKLIB/issues/392)), and with `l6_merge` an epoch in which one channel already has the new clock no longer drops to Single ([#380](https://github.com/h-shiono/MRTKLIB/issues/380)). On live replays the fix rate rises by up to 6 points (92.2 % → 98.4 % on the worst hour) and Float errors of tens of metres disappear; claslib post-processing is byte-identical. New RT regression on a live capture with frame losses (dataset `clas_l6gap-v1`). `raw_t` and `clas_l6buf_t` gain fields (ABI change); new API `clas_set_l6tag()`. The regression suite is 191 / 191.

**Next:** GPS with PPP-B2b without broadcast GPS code biases; phase-bias discontinuities signalled to the RTK engine as MSM lock-time resets in `mrtk cssr2rtcm3` ([#453](https://github.com/h-shiono/MRTKLIB/issues/453)); real-time CLAS fix loss after a handover and wrong ambiguity sets after a reset ([#389](https://github.com/h-shiono/MRTKLIB/issues/389), [#394](https://github.com/h-shiono/MRTKLIB/issues/394)); extend the CLAS-style CSSR dump to MADOCA / Galileo HAS / BeiDou B2b ([#229](https://github.com/h-shiono/MRTKLIB/issues/229)); investigate the HAS absolute-accuracy offset ([#215](https://github.com/h-shiono/MRTKLIB/issues/215))

> [!NOTE]
> demo5 algorithm improvements are adapted from **[demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB)**
> by Tim Everett (rtklibexplorer).  Benchmark results use the
> [PPC-Dataset](https://github.com/taroz/PPC-Dataset) (Taro Suzuki, Chiba Institute of Technology)
> for kinematic positioning and the
> [Google Smartphone Decimeter Challenge 2023](https://www.kaggle.com/competitions/smartphone-decimeter-2023)
> dataset for smartphone SPP.

### Known Limitations

| Mode | L6E (SSR) | L6D (CLAS) | Notes |
|------|-----------|------------|-------|
| **Post-processing** (`mrtk post`) | Multiple `.l6` files | Dual-channel | Full PPP/PPP-AR/PPP-AR+iono/PPP-RTK |
| **Real-time** (`mrtk run`) | Single stream (`inpstr3`) | Dual-channel (`inpstr2` + `inpstr3`) | PPP/PPP-AR/PPP-AR+iono, PPP-RTK with 1ch or 2ch CLAS L6D |

* **Real-time CLAS L6D**: Dual-channel support uses stream 3 for L6 ch1 and stream 2 (base slot, unused in PPP-RTK) for L6 ch2.
* **Real-time L6E**: The rtksvr provides a single correction input (`inpstr3`). Multiple QZSS L6E channels (e.g., QZS-3 and QZS-4) are supported when the receiver multiplexes them into one SBF stream.
* **Galileo HAS**: float PPP on GPS and Galileo. On the regression dataset the solution sits ~0.6 m horizontally from the reference coordinate; the cause is under investigation ([#215](https://github.com/h-shiono/MRTKLIB/issues/215)).
* **BeiDou PPP-B2b**: float PPP on BeiDou only. The broadcast has GPS orbit and clock corrections but no GPS code biases.
* **SouthPAN PVS**: float PPP on GPS L1 C/A + L5 and Galileo E1 + E5a; forward solutions only in `mrtk post`. The decoder handles the current MT32 messages on L5. SouthPAN plans to move PVS to a new signal, L5b (1207.14 MHz), with a new message format on new GEOs, which will need a new decoder. DAS log input is not validated ([#462](https://github.com/h-shiono/MRTKLIB/issues/462)).
* **Real-time input for HAS, PPP-B2b and PVS**: Septentrio SBF only (see the [capability matrix](docs/reference/correction-services.md#inputs)).

## Live Dashboard

Real-time CLAS PPP-RTK positioning performance can be monitored on a public Grafana dashboard:

> **[CLAS Summary Dashboard](https://live.pntmoni.com/)**

The dashboard shows fix rate, ENU accuracy, satellite visibility, and correction age
streamed from an active `mrtk run` session.

---

## Related Projects

Ready-to-use packages for trying MRTKLIB without building it from source:

| Project | Description |
|---------|-------------|
| **[mrtklib-docker-ui](https://github.com/h-shiono/mrtklib-docker-ui)** | Web UI for MRTKLIB in a Docker container — post-processing (`mrtk post`) and real-time positioning (`mrtk run`) from the browser, with TOML import / export and result visualization. No compilation required: `docker compose up`. |
| **[mrtklib-quickstart](https://github.com/h-shiono/mrtklib-quickstart)** ([docs](https://h-shiono.github.io/mrtklib-quickstart/), 日本語 / English) | One-script quickstart for real-time MADOCA-PPP with a Septentrio mosaic-G5 receiver — configures the receiver, launches the Docker web UI, and includes teaching material for Windows / macOS / Linux. |

## Getting Started (How to Build)

### Prerequisites
* CMake (>= 3.15)
* C11 compatible compiler (GCC, Clang)
* LAPACK/BLAS (optional; e.g. `liblapack-dev` on Ubuntu, the Accelerate framework on macOS). It must be an **LP64** (32-bit integer) build; see the [installation guide](docs/getting-started/install.md#lapack-and-blas-lp64-interface).

### Build Instructions
MRTKLIB uses standard CMake workflow. Out-of-source builds are strictly recommended.

```bash
# Clone the repository
git clone https://github.com/h-shiono/MRTKLIB.git
cd MRTKLIB

# Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the library and apps
cmake --build build -j
```

The unified `mrtk` binary will be located in the `build/` directory.

### Running Applications
Configuration files (TOML) are stored in the `conf/` directory. The repository includes a one-hour CLAS sample (2019-08-27, a station in Japan), which the commands below use:

```bash
# Unpack the CLAS sample data
tar xzf tests/data/claslib/claslib_testdata.tar.gz -C tests/data/claslib

# Post-processing: CLAS PPP-RTK over one hour (NMEA output)
./build/mrtk post -k conf/claslib/rnx2rtkp.toml \
    -ts 2019/08/27 16:00:00 -te 2019/08/27 16:59:59 -o clas.nmea \
    tests/data/claslib/0627239Q.obs tests/data/claslib/sept_2019239.nav tests/data/claslib/2019239Q.l6

# Real time: the same hour replayed at 10x speed through mrtk run
# (output: tests/data/claslib/out/rtkrcv_clas_test.nmea; type "shutdown" to stop)
mkdir -p tests/data/claslib/out
./build/mrtk run -s -o conf/claslib/rtkrcv.toml
```

For the other services and for live receivers, see the quick starts under [`docs/guide/`](docs/guide/).

## For Developers

### Development Workflow

MRTKLIB is developed with AI assistance.
Algorithm porting, tests, code migration and documentation are written with
**[Claude Code](https://claude.ai/claude-code)** (Anthropic); commits made with it
carry a `Co-Authored-By: Claude` trailer. Porting strategy was also discussed with
**Gemini Pro** (Google).

The project author sets the architecture and implementation strategy, reviews every
change, runs the regression suite and merges it.

### Running Tests
MRTKLIB has unit tests (utest) and regression tests; the regression tests compare against upstream outputs and independent reference coordinates (see the [test methodology](docs/reference/test-accuracy-methodology.md)).

```bash
cd build
ctest --output-on-failure
```

### Generating Documentation
The public API is documented with Doxygen comments. To generate the API reference and call graphs:

```bash
cmake --build build --target doc
```
Documentation will be generated in `build/doc/html/index.html`.

## How to Cite

If you use MRTKLIB in your research or product, please cite it via its archived
release on [Zenodo](https://zenodo.org/). You can cite **all versions** by using
the concept DOI `10.5281/zenodo.20373746`; Zenodo also mints a version-specific
DOI for each individual release.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.20373746.svg)](https://doi.org/10.5281/zenodo.20373746)

```bibtex
@software{mrtklib,
  author    = {Shiono, Hayato},
  title     = {{MRTKLIB: Modernized RTKLIB for Next-Generation GNSS}},
  publisher = {Zenodo},
  doi       = {10.5281/zenodo.20373746},
  url       = {https://doi.org/10.5281/zenodo.20373746}
}
```

## License & Attributions
MRTKLIB is distributed under the BSD 2-Clause License.

This project stands on the shoulders of giants:

| Contributor | Role |
|-------------|------|
| **Tomoji Takasu** | RTKLIB — the foundational GNSS positioning library |
| **Taro Suzuki** | RTKLIB — u-blox receiver decoder |
| **Tim Everett (rtklibexplorer)** | demo5 RTKLIB — kinematic RTK algorithm improvements (PAR, detslp_dop/code, varerr) |
| **Geospatial Information Authority of Japan** | GSILIB v1.0.3 — CLAS grid correction algorithms |
| **Mitsubishi Electric Corp.** | CLASLIB — CLAS PPP-RTK / VRS-RTK engine |
| **Japan Aerospace Exploration Agency** | MALIB — MADOCA-PPP structural base |
| **TOSHIBA ELECTRONIC TECHNOLOGIES** | MALIB + MADOCALIB — MADOCA-PPP engine and L6E/L6D decoder |
| **Cabinet Office, Japan** | MADOCALIB — PPP/PPP-AR positioning algorithms |
| **Lighthouse Technology & Consulting** | MADOCALIB — system integration and L6E/L6D decoder |

For detailed licensing information, please refer to [LICENSE](LICENSE).

## Benchmark Datasets

**Kinematic (geodetic).** The **PPC-Dataset** (Precise Positioning Challenge
2024), kindly released as open data by:

> **Taro Suzuki**, Chiba Institute of Technology
> <https://github.com/taroz/PPC-Dataset>

**Smartphone (SPP).** The **Google Smartphone Decimeter Challenge 2023**
(GSDC-2023), released on Kaggle by Google for the ION GNSS+ conference and used
here under the Kaggle competition rules:

> <https://www.kaggle.com/competitions/smartphone-decimeter-2023>

See [docs/reference/benchmark.md](docs/reference/benchmark.md) (kinematic) and
[docs/reference/benchmark-gsdc.md](docs/reference/benchmark-gsdc.md) (smartphone)
for usage instructions.
