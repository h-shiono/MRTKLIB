# mosaic-G5 open-service SSR dataset (DOY 2026-296, hour u)

Release asset `g5p3_296u-v1.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386). The
`b2b_*` and `rtkrcv_b2b*` tests use it (#417), and it is cut so that SouthPAN
PVS tests (#418) can use it too. Built by
`scripts/tests/make_g5p3_296u_fixture.py`. Only this README and the test
configuration template `b2b_g5p3.toml.in` are committed.

SHA256 `923b2fe8e276664b8ad809947eaca2afc5577a9eca39cb7274b50781b5d44106`
(the build is reproducible: same capture and navigation file, same hash).

It is a small regression dataset, not a performance baseline. The window is
one hour, so BeiDou-only float PPP is still converging at its end. The hour
also has the receiver's 1-2 s output drops (see `../clas_l6gap/README.md`).

## Capture

The maintainer's live kubolab session, mosaic-G5 (G5P3), file `rovr_296u.sbf`.
The antenna and reference coordinate are the same as in the handover and
clas_l6gap datasets: TOPCON CR-G5 without radome (`TPSCR.G5        NONE`), see
[`../handover/README.md`](../handover/README.md).

Window: GPST week 2441, TOW 503970-507600 (2026-10-23 19:59:30-21:00:00). It
starts just before the top-of-hour BeiDou B-CNAV1 and GPS ephemeris updates,
so a replay has broadcast ephemeris within the first minute.

## Files

| File | Bytes | Content |
|---|---|---|
| `G5P3296u.sbf` | 13780172 | SBF blocks below, valid CRC only |
| `G5P3296u.sbf.tag` | 243740 | synthetic tag for `mrtk run` file replay (`make_handover_fixtures.write_tag`) |
| `BRD4296u.rnx` | 519180 | RINEX 4 broadcast navigation cut from `BRD400DLR_S_20262960000_01D_MN.rnx` (IGS merged broadcast ephemeris, DLR) |

SBF blocks kept (counts in the window):

| Block | Count | Use |
|---|---|---|
| 4027 MeasEpoch | 3557 | observations, 1 Hz |
| 4007 PVTGeodetic | 3557 | receiver PVT |
| 4017 GPSRawCA | 5690 | GPS LNAV subframes. GPSNav is output only when the 2-hourly ephemeris changes, so a replay needs the raw subframes. |
| 5891 GPSNav / 4002 GALNav / 4081 BDSNav | 10 / 58 / 18 | decoded ephemeris. Galileo changes every 10 min, so no Galileo raw navigation is kept. |
| 4251 BDSCNav1 | 13 | BeiDou B-CNAV1 ephemeris, which the PPP-B2b IODN refers to |
| 4242 BDSRawB2b | 13996 | PPP-B2b frames of the GEOs C01-C04 only. The IGSO/MEO frames (B-CNAV3) are dropped. |
| 4021 GEORawL5 | 3559 | SouthPAN PRN 122 only (DFMC: MT32 x2599, MT0, 31, 37, 39, 40, 42, 47, 62) |

`BRD4296u.rnx` keeps the records whose epoch is inside the window plus or
minus the update interval: BDS CNV1 and GAL INAV +-1 h (225 / 401 records),
GPS LNAV +-2 h (101 records). Post-processing needs it because `mrtk convert`
writes no BeiDou CNV1 records.

## PPP-B2b content

The C01-C04 frames carry MT1/2/3/4/63 (no MT5-7) with the same mask (IODP
14, 62 satellites), but two streams: C01-C03 send IOD SSR 1 and C04 IOD SSR 2.
The decoder takes one GEO at a time (the first to send a mask). The streams have
orbit corrections for 12 BDS-3 satellites (C06-C42) and 15 GPS satellites
in the hour (the mask lists 30 BDS and 32 GPS), and code biases (modes 0, 1,
2, 4, 5, 7, 8, 12) for BeiDou only. Without GPS code biases the tests are
BeiDou-only float PPP (B1I/B3I).
