# CLAS L6 frame-gap regression dataset

Release asset `clas_l6gap-v1.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386), used by the
`rtkrcv_clas_l6gap_*` tests (#363). Built by `scripts/tests/make_clas_l6gap_fixture.py`;
nothing here is committed except this README.

SHA256 `365fe0466a64badb3f52ccebe85ace855615d08e8908848ba1999323cad7bc1e`

Capture: the maintainer's live `mrtk run` session of DOY 2026-296 (2026-10-23),
mosaic-G5 (G5P3) SBF logged by the host, hour u (`rovr_296u.sbf`). Same antenna
as the handover dataset (TOPCON CR-G5 without radome, `TPSCR.G5        NONE`;
see [`../handover/README.md`](../handover/README.md) for the reference
coordinate, which the replay's fixed epochs match to < 1 mm in height).

The host stalled every few minutes and the receiver dropped its USB output
buffer: all SBF blocks of 1-2 s are missing, the QZSRawL6D frames included. A
frame lost inside a CLAS subframe shifted the bits of the following frames,
and the decoder read garbage corrections from them (#363). In this window that
gave Float excursions of 6-27 m in three places (#381 reset loop); with the
frame-time check (`clas_set_l6tag`) the subframe is discarded instead.

Window: GPST week 2441, TOW 504300-505500 (20:05-20:25), `G5P3296u_20m.sbf`
(6036380 bytes) and its synthetic `.tag` (`make_handover_fixtures.write_tag`).

SBF blocks kept: the observation / navigation set of the handover dataset
(4027, 4007, 4017, 4022, 4023, 4066, 5891, 4002, 4095) plus QZSRawL6D (4270).

| L6D PRN | Frames | Seconds without a frame | Subframes with a frame missing inside |
|---|---|---|---|
| 194 | 1171 | 29 | 11 |
| 195 | 1172 | 28 | 11 |
| 199 | 1170 | 30 | 13 |
| 200 (MADOCA iono) | 1172 | 28 | 1 |
| 201 (MADOCA iono) | 1172 | 28 | 0 |
