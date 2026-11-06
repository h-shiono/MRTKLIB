# SouthPAN PVS in-area dataset (2025-233 h, 2025-110 f)

Release asset `southpan-v1.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386). The
`pvs_alby_*` and `pvs_alic_*` tests use it (#418). Built by
`scripts/tests/make_southpan_fixture.py`; only this README is committed.

SHA256 `cf55f6ce2ffb900606078cd2366d0ccf4a843fb7707c8709314befb3e73ddefe`
(the build is reproducible: same inputs, same hash).

It is a small regression dataset, not a performance baseline. Each case is one
hour, shorter than the 80 min IOC convergence target of PVS (SBAS-STN-0001
Rev 04 §8.1.4).

## Sources and licences

The asset carries the full attribution in `NOTICE.txt`.

- Observations and correction logs: [cssrlib-data](https://github.com/hirokawa/cssrlib-data)
  (MIT, © 2022 Rui Hirokawa), commit `fd038eb`, `data/doy2025-233/` and
  `data/doy2025-110/`. Later commits replace `233h_sbas.txt` (562acb8) with a
  log whose PRN 122 records differ, so the build needs that commit.
- Observations of ALBY / ALIC: Geoscience Australia stations (IGS network),
  Australian National Positioning Infrastructure Data Collection, CC BY 4.0.
- SouthPAN messages (SIS and DAS): Commonwealth of Australia (Geoscience
  Australia) and Toitū Te Whenua LINZ, CC BY 4.0 (SBAS-STN-0001 Rev 04,
  "Licence").
- Broadcast navigation: `BRD400DLR_S_2025{233,110}0000_01D_MN.rnx`,
  Montenbruck & Steigenberger (2022), DLR/GSOC,
  [doi:10.57677/BRD400DLR](https://doi.org/10.57677/BRD400DLR); IGS Terms of Use.

cssrlib-data's own `alby233h_rnx.nav` is not used: it is written by cssrlib
and its Galileo F/NAV records carry the I/NAV data-source value (517), so
MRTKLIB selects no F/NAV ephemeris from it.

## Files

| File | Content |
|---|---|
| `ALBY233h.obs` | ALBY (Albany, AU), Septentrio PolaRx5, TWIVC6050 NONE, 2025-08-21 07:00-08:00 GPST, 1 Hz, RINEX 4.02 |
| `ALBY233h.sbs` | SouthPAN PRN 122 L5 messages logged by a mosaic-X5 in Kamakura (SIS), RTKLIB SBAS log form with the L5 type column |
| `BRD4233h.rnx` | cut of `BRD400DLR_S_20252330000_01D_MN.rnx` |
| `ALIC110f.obs` | ALIC (Alice Springs, AU), TWIVC6050 NONE, 2025-04-20 04:59:31-06:00:30 GPST, 1 Hz, RINEX 3.04 |
| `DAS2025110f.txt` | SouthPAN Data Access Service L5 log (SISNeT `*MSG` lines, SINCA), unchanged; the test converts it with `scripts/tests/das2sbs.py` |
| `BRD4110f.rnx` | cut of `BRD400DLR_S_20251100000_01D_MN.rnx` |
| `NOTICE.txt` | sources and the cssrlib-data MIT licence |

Reductions: observations are GPS and Galileo only with the 1C and 5Q
observation types; navigation keeps GPS LNAV records within the hour +- 2 h
and Galileo F/NAV +- 4 h (the ephemeris age limits); the SIS log keeps the
PRN 122 L5 frames only. The Kamakura log also has PRN 122 L1 frames and the
BDSBAS L5 GEOs PRN 130/143/144, which send MT32 for their own service.

## Reference coordinates

The RINEX header positions, as cssrlib's `test_ppppvs.py`:

| Station | X | Y | Z (m) |
|---|---|---|---|
| ALBY | -2441715.2741 | 4629128.6896 | -3633362.5218 |
| ALIC | -4052052.9320 | 4212835.9496 | -2545104.3074 |

Both agree with the IGS weekly SINEX at the marker to about 1 cm (reference
minus SINEX, dE / dN / dU):

| Station | SINEX (estimate epoch) | dE / dN / dU (m) |
|---|---|---|
| ALBY | `IGS0OPSSNX_20252290000_07D_07D_SOL.SNX` (2025:232.5) | -0.001 / -0.002 / -0.005 |
| ALIC | `IGS0OPSSNX_20251100000_07D_07D_SOL.SNX` (2025:113.5) | -0.006 / -0.010 / +0.000 |

On ALIC both MRTKLIB and cssrlib end the hour about 1 m from the reference
(mostly up). Float PPP of the same observations with CODE final products
(`COD0OPSFIN`, Galileo E1/E5a only, whose clocks and OSB match these signals)
ends within 0.17 m (kinematic) / 0.06 m (static), and ALBY within 0.04 m, so
the offset comes from the PVS corrections of that hour as logged by the DAS:
GPS-only and Galileo-only PVS are each 0.5-0.7 m off. It does not move with
the cut navigation file (full DLR file: same result) or with a ±18 s shift of
the DAS time tags (< 0.1 m). There is no SIS log of that hour to tell the DAS
stream from the broadcast one. DAS input is not validated (out of the scope of
#418, tracked in #462); the ALIC gate only catches a broken DAS input path.

## Measured (2026-11-06, `igs14_L5copy.atx`, mean of the last 20 min)

| Case | dE / dN / dU (m) | satellites |
|---|---|---|
| ALBY kinematic | -0.008 / -0.046 / +0.119 | 11.7 |
| ALBY static | +0.058 / -0.050 / +0.060 | 11.7 |
| ALIC kinematic | -0.580 / -0.196 / -0.889 | 11.0 |

cssrlib `test_ppppvs.py` (static, same inputs, igs20.atx): ALBY 0.10 m 2D /
0.53 m U rms, ALIC 0.29 / 0.78 m over the last 20 min.
