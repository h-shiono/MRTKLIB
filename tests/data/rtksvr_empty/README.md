# Empty rover epoch regression dataset

Release asset `rtksvr_empty-v1.tar.gz` of
[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386), used by the
`rtkrcv_rt_empty_epoch` tests (#382). Only this README and the conf template
`rtkrcv_empty_epoch.toml.in` are committed.

SHA256 `3ccc8f3ca8b4711356f41ce9ba6ba43ea5823fbb022452480e88b8b49ce01f0a`

Capture: the maintainer's simpleRTK4 Pro (Septentrio mosaic-G5 P3) with a
u-blox ANN-MB2 antenna, site FUET, DOY 2026-284 (2026-10-11), hour f. The
receiver logged SBF to the host. A signal blockage of about 50 s emptied the
MeasEpoch blocks:

| TOW (GPST week 2440) | Satellites in MeasEpoch |
|---|---|
| 21300-21398 | 40-55 |
| 21399-21407 | 1 (decodes to no observations) |
| 21408-21448 | 0 |
| 21449-21499 | 17, then back to 50+ |

File: `FUET284f_200s.sbf` (351788 bytes), TOW 21300000-21500000 ms, cut with
`scripts/tests/sbf_filter.py --blocks 4027,4017 --start-tow 21300000
--duration 200`. Blocks kept: MeasEpoch (4027) and GPSRawCA (4017). No L6, no
time tag: the test replays the file as fast as it reads.

Before the fix, `mrtk run` passed each empty epoch to `rtkpos()`. The solution
was written again under the time of TOW 21398 (05:56:20 UTC): 51 identical GGA
times. After the fix, the GGA times increase strictly and nothing is written
between 05:56:21 and 05:57:10 UTC.
