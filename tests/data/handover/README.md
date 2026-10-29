# QZSS L6 handover regression dataset

Release asset `handover-v1.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386), used by the
handover regression tests (#372). Built by `scripts/tests/make_handover_fixtures.py`;
nothing here is committed except this README.

SHA256 `2690ac610a3696731dcfa629d2f98a9dfd54fef9126ae9b6de5c3349a3792fe9`

Captures: maintainer recordings of DOY 2026-286 (2026-10-13), one antenna
(TOPCON CR-G5 without radome, `TPSCR.G5        NONE`) split to a mosaic-G5 (G5P3),
a NEO-D9C (D9C1, L6 only) and a mosaic-CLAS (MCLA; the capture files are
misnamed `MCLA0286*`). Each window is handover - 600 s .. + 300 s.

SBF blocks kept for observations/navigation: 4027,4007,4017,4022,4023,4066,5891,4002,4095
(MeasEpoch, PVTGeodetic, GPS C/A, Galileo F/NAV + I/NAV, QZSS C/A, decoded
GPS/Galileo/QZSS ephemerides); MCLA also keeps 4069 (QZSRawL6). D9C keeps
UBX NAV-PVT + RXM-QZSSL6. Every file has a synthetic `.tag` on a common
GPST axis (see the script docstring).

| Case | Files | Source | Window (TOW s) | Bytes | L6 channels (PRN: frames, first..last TOW) |
|---|---|---|---|---|---|
| d9c_m | `G5P3286m_15m.sbf` | G5P3286m.sbf | 218178–219078 | 2532944 | — |
| d9c_m | `D9C1286m_15m.ubx` | D9C1286m.ubx | 218178–219078 | 669328 | ch0: 194: 900, 218178.5..219077.5; ch1: 199: 599, 218178.5..218776.5 → 195: 300, 218778.5..219077.5 |
| d9c_s | `G5P3286s_15m.sbf` | G5P3286s.sbf | 240303–241203 | 3016196 | — |
| d9c_s | `D9C1286s_15m.ubx` | D9C1286s.ubx | 240303–241203 | 669056 | ch0: 194: 598, 240303.5..240900.5 → 196: 300, 240903.5..241202.5; ch1: 195: 900, 240303.5..241202.5 |
| mcla_a | `MCLA286a_15m.sbf` | MCLA286a.sbf | 175474–176374 | 2700876 | ch0: 195: 597, 175474.0..176070.0 → 199: 300, 176074.0..176373.0 |
| mcla_e | `MCLA286e_15m.sbf` | MCLA286e.sbf | 189766–190666 | 2293596 | ch0: 199: 597, 189766.0..190362.0 → 194: 300, 190366.0..190665.0 |
| mcla_o | `MCLA286o_15m.sbf` | MCLA286o.sbf | 225343–226243 | 2316500 | ch0: 194: 597, 225343.0..225939.0 → 195: 300, 225943.0..226242.0 |

Purpose: RT replay (`mrtk run`) and post-processing (`mrtk convert` +
`mrtk l6extract` + per-channel concatenation + `mrtk post`) through a CLAS
L6 handover: recovery to fix, no solution freeze (#205/#218), satellite count.
`handover_manifest.json` (in the asset) holds the same data for the tests.

## Reference coordinate

Antenna reference point of the CR-G5, ITRF at the observation epoch, used by the
`post_handover_*_abs_check` tests:

    35.666341805  139.792211221  59.7614   (lat deg, lon deg, ellipsoidal height m)

Static PPP-AR over the full day 2026-10-13 (G5P3286a..x, 30 s, GPS + Galileo,
`conf/igs/rnx2rtkp_igspppar.toml` with the antenna above and the full igs20.atx)
with CODE MGEX final products (`COD0MGXFIN_20262860000_01D_*`: 05M ORB, 30S CLK,
OSB, ERP): median of the 1747 fixed epochs out of 2880. Cross-checks:

- JAXA final (`JGX0OPSFIN`), same processing: +0.6 / -5.4 / +14.2 mm (E/N/U).
- GPS-only with CODE: -0.6 / +0.1 / -1.8 cm (igs20.atx has no Galileo
  frequencies for `TPSCR.G5 NONE`; MRTKLIB fills them from the GPS values).
- The maintainer's coordinate of the antenna, from RTK to a nearby station:
  -1.1 / +0.8 / +5.5 cm (a systematic RTK-vs-PPP offset is expected; the PPP
  solution is the reference).

The ZED-F9P ECJ02 data elsewhere in the test suite uses a different antenna
(Trimble Zephyr 2, ~1.2 m away).
