# Galileo HAS dataset (has-v2)

Release asset `has-v2.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386),
extracted here by the `has_setup` fixture. The `has_*`, `has296_*` and
`rtkrcv_has*` tests use it. Only this README and the test configuration
template `has_g5p3.toml.in` are committed.

SHA256 `da75f5e8ab2cafe3c150d498b8dfa5a901fe32bebe5bfbc96e9cc3a0f2bf581b`.
`scripts/tests/make_has_v2_fixture.py` builds it reproducibly (same capture,
same hash).

has-v2 is a superset of has-v1: it adds the 2026-10-23 hour for the #473
regression tests (Galileo HAS code biases) and the first real-time HAS test.

## Files

| File | Bytes | Content |
|---|---|---|
| `G5P3162a_15m.sbf` | 3366892 | the has-v1 member, byte-identical (2026-06-10, 15 min; see `docs/design/gal-has.md` §11) |
| `G5P3296u_has.sbf` | 15288404 | one hour of the 2026-10-23 capture, blocks below, valid CRC only |
| `G5P3296u_has.sbf.tag` | 544068 | synthetic tag for `mrtk run` file replay (`make_handover_fixtures.write_tag`) |

## G5P3296u_has.sbf

The maintainer's live kubolab session, mosaic-G5 (G5P3), file `rovr_296u.sbf`,
the same capture as the `g5p3_296u` dataset with a different block set.
Window: GPST week 2441, TOW 503970-507600 (2026-10-23 19:59:30-21:00:00).

| Block | Count | Use |
|---|---|---|
| 4027 MeasEpoch | 3557 | observations, 1 Hz |
| 4007 PVTGeodetic | 3557 | receiver PVT |
| 4017 GPSRawCA | 5690 | GPS LNAV subframes (GPSNav is output only when the 2-hourly ephemeris changes) |
| 5891 GPSNav / 4002 GALNav | 10 / 58 | decoded ephemeris |
| 4022 GALRawFNAV / 4023 GALRawINAV | 2643 / 26024 | Galileo raw navigation. Without them the slice lacks ephemeris sets (E13 and E26 for most of the hour) and its solution differs from the full capture's. |
| 4024 GALRawCNAV | 26460 | E6-B C/NAV pages: 13838 valid HAS records, 12622 dummy pages, no CRC failure |

Equivalence with the full capture, `mrtk post` with `conf/has/ppp_gal_has.toml`:
the `.has` from the slice is byte-identical to the full capture's over the
window, and the PPP solutions are bit-identical over all 3549 common epochs.

## Reference coordinate and antenna

TOPCON CR-G5 without radome (`TPSCR.G5        NONE`), the antenna and
reference coordinate of the handover, clas_l6gap and g5p3_296u datasets: the
antenna reference point from a full-day static PPP-AR of 2026-10-13 with CODE
MGEX final products, see [`../handover/README.md`](../handover/README.md).

    35.666341805  139.792211221  59.7614   (lat deg, lon deg, ellipsoidal height m)
    -3961905.1685  3348992.7560  3698212.4291   (ECEF m)

`has_g5p3.toml.in` applies the antenna model (`igs14_L5copy.atx` from the
claslib fixture), so the solution refers to the same point. Without it the
height is about 5 cm higher. The has-v1 tests keep their own reference (a
MADOCA-PPP solution of 2026-06-12 without the antenna model, about 7.6 cm
above this ARP).

## Measured (2026-11-23)

Last 5 min against the reference, GPS + Galileo float PPP:

| | H95 | V95 | GPS / Galileo sats per epoch | mean ns |
|---|---|---|---|---|
| post, before #473 | 0.244 m | 0.535 m | 6.21 / 0.00 | |
| post, #473 fixed | 0.154 m | 0.273 m | 6.21 / 5.65 | |
| replay x20, before #473 | 0.245 m | 0.533 m | | 6.21 |
| replay x20, #473 fixed | 0.153 m | 0.273 m | | 11.85 |

The gates are twice the HAS service targets (H95 0.40 m, V95 0.80 m) plus the
satellite usage (post: GPS >= 4, Galileo >= 3 per epoch from the `-y 2` status
file; replay: mean ns >= 9).
