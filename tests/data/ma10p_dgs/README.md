# DATAGNSS MA-10P (DGS) dataset

Release asset `ma10p_dgs-v1.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386), used by the
`rtkrcv_rt_dgs_clas` tests (#570). Only this README and the conf template
`rtkrcv_dgs_clas.toml.in` are committed.

SHA256 `72c325c2359c745359a207e2b88acf70a226c59dcafedc83813cce0d29232442`

Captures of the DATAGNSS MA-10P: a D10P (L1 + L5 GNSS, RTCM3 output) and a
QZS-6C (QZSS L6, DGS output) behind one antenna. Recorded and prepared by
DATAGNSS, published here with their permission
([#579](https://github.com/h-shiono/MRTKLIB/pull/579#issuecomment-7155684783)).
The DGS format is described in `src/data/rcv/mrtk_rcv_dgs.c` and
`docs/hardware/clas-ma10p.md`.

## D203329g: CLAS (L6D firmware), 30 min

Vendor station 203, static, open sky. GPST 2026-11-25 06:06:00 to 06:36:00
(week 2446, TOW 281160 to 282960).

Antenna phase centre, given by the vendor (derived from the GSI F5.1 solution,
ITRF2020, epoch 2026-09-30):

| | |
|---|---|
| Latitude, longitude | 34.691885173°, 135.545748367° |
| Ellipsoidal height | 50.6173 m |
| ECEF | X −3747500.9731, Y 3676781.7552, Z 3609843.2370 m |

| File | Content |
|---|---|
| `D203329g_30m.rtcm3` | D10P: MSM7 1077 (GPS 1C/5Q), 1087 (GLONASS 1C), 1097 (Galileo 1X/5Q), 1117 (QZSS 1C/5Q), 1127 (BeiDou 2I/1P/5P) at 1 Hz; ephemerides 1019, 1020, 1042, 1044, 1046. Station messages removed by the vendor |
| `D203329g_30m.dgs` | QZS-6C: 5397 DGS frames, nothing else. L6D PRN 194: 1800 frames, 195: 1797, 196: 1800. 48 carry the Reed-Solomon flag (194: 1, 195: 46, 196: 1) |
| `D203329g_5m.{rtcm3,dgs}` | the first 5 min (TOW 281160 to 281459, 900 DGS frames), cut at tag tick 300 s; input of the test |
| `*.tag` | time tags for `mrtk run` file replay (`::T`) |

PRN 194 and 196 carry the same CLAS transmit pattern (identical data parts in
every second both are clean); PRN 195 carries the other.

The receiver gave no arrival times. The vendor synthesised the tags: RTCM3
messages at their MSM epoch + 0.2 s, DGS frames at the frame time + 1.0 s (end
of the 1 s frame), both tag headers starting at GPST 2446/281160. The 5 min
files are byte prefixes of the 30 min files, with the tag records up to tick
300 s.

Replay of the 30 min files with `conf/claslib/rtkrcv_rtcm3_ubx.toml` adapted as
in `rtkrcv_dgs_clas.toml.in` (`::T::x10`): Fix / Float / Single 1412 / 344 / 44,
first Fix after 16 s, fixed solutions at E +3.5 / N −1.9 / U −4.8 cm from the
antenna position (the vendor's result, reproduced). The 5 min test input gives
285 / 5 / 11, first Fix after 16 s. `mrtk post` on the same data fixes at wrong
positions (#585).

## DGE1349j: MADOCA-PPP (L6E firmware), 5 min

Site not named by the vendor. GPST 2025-12-15 09:20:00 to 09:25:00 (week 2397,
TOW 120000 to 120299). Not used by a test yet.

| File | Content |
|---|---|
| `DGE1349j_5m.dgs` | 899 DGS frames, nothing else. L6E PRN 204: 300 frames, 205: 299 (one with the Reed-Solomon flag), 209: 300 |
| `DGE1349j_5m.rtcm3` | MSM7 1077 (GPS 1C/2L/5Q), 1097 (Galileo 1C/5Q/7Q), 1117 (QZSS 1C/5Q/2L), 1127 (BeiDou 2I/7I/5P) at 1 Hz, ephemerides 1019, 1042, 1044, 1046. Not the D10P signal set: the vendor did not say which receiver gave the observations |
| `*.tag` | synthesised in MRTKLIB: DGS frames at the frame time + 1.0 s, RTCM3 messages at their MSM epoch + 0.15 s, both tag headers starting at GPST 2397/120000 |

`mrtk l6extract -r dgs` gives 300 / 298 / 300 frames (1 skipped). MADOCA-PPP
float PPP in `mrtk post` and in a `mrtk run` replay.
