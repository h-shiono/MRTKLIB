# CLAS support files — ANTEX and ERP provenance

**Status: record of CLASLIB 0.9.1 (2026-11-06), examined 2026-11-09.**
CLASLIB 0.9.1 changed no code: it only replaced `igs14_L5copy.atx` and
`igu00p01.erp`, and neither file says how it was built. This page records how
they were built, as worked out from the files themselves, and how MRTKLIB
regenerates them.

The tools are `scripts/tools/make_clas_atx.py` and
`scripts/tools/update_igu_erp.py`. For usage see
[Quick Start: CLAS PPP-RTK](../guide/quickstart-clas.md#refreshing-the-antex-and-erp-files).

!!! note "The test data still uses the CLASLIB 0.9.0 files"
    The files in `tests/data/claslib/` are the CLASLIB 0.9.0 versions.
    Moving to newer files changes the positioning inputs, so it needs its own
    before/after comparison and regenerated references.

---

## 1. ANTEX: `igs14_L5copy.atx`

### 1.1 What the file is

Despite the name, the file is an **igs20** ANTEX. The CLASLIB 0.9.1 file is
`igs20_2437.atx` from the IGS archive with one kind of change: a G05 block copied
from G02. All other antennas, the header and the antenna order are unchanged.

The name is left over from an earlier base file. The `SINEX CODE` of each release:

| CLASLIB | Date | `SINEX CODE` of `igs14_L5copy.atx` |
|:--|:--|:--|
| 0.7.3a | 2024-04-23 | `IGS14_1984` |
| 0.7.4 to 0.9.0 | 2024-11-25 to 2026-10-09 | `IGS20_2317` |
| 0.9.1 | 2026-11-06 | `IGS20_2437` |

The file therefore moved to igs20 before the interface specification did.
IS-QZSS-L6 defined the CLAS coordinate as ITRF2014 (IGb14) up to at least the
-005 edition, and -008 (2026-03-18, §5.2.2) changed it to ITRF2020 (IGb20). The
0.9.0 file has the same list of antennas as 0.9.1.

### 1.2 Why L2 is copied to L5

Several ground antennas have no GPS L5 calibration in igs20. The ANTEX reader
leaves a frequency without a block at zero, so L5 on those antennas would get no
phase-centre correction at all. CLASLIB does not document the reason for the
copy, but filling that gap with the L2 values is its evident effect.

### 1.3 The rule

For each listed antenna, a G05 block is inserted right after the G02 block. Its
values are the G02 values, unchanged. `# OF FREQUENCIES` is not incremented, the
line endings are CRLF, and no comment line is added.

| Antenna (type + radome) | Calibrated G05 in igs20 | Effect of the copy |
|:--|:--|:--|
| `TOP700779A NONE` | no | L5 uses the L2 values |
| `TPSCR.G5 NONE` | no | L5 uses the L2 values |
| `TPSCR.G5C TPSH` | no | L5 uses the L2 values |
| `TPSCR4 NONE` | no | L5 uses the L2 values |
| `TRM159900.00 SCIS` | no | L5 uses the L2 values |
| `TRM23903.00 NONE` | no | L5 uses the L2 values |
| `TRM29659.00 SCIS` | no | L5 uses the L2 values |
| `TPSCR.G5 TPSH` | yes | none |
| `TRM29659.00 NONE` | yes | none |
| `TRM59800.00 SCIS` | yes | none |
| `TRM59800.80 NONE` | yes | none |
| `TRM59800.80 SCIS` | yes | none |

On the last five the copy has no effect: the calibrated G05 block comes later in
the antenna, and the readers in both CLASLIB and MRTKLIB overwrite on each block.

### 1.4 Difference from the CLASLIB file

CLASLIB also copies G02 to G05 on one satellite antenna, `BLOCK IIA` / `G01`
(valid from 1992-11-22). The copy is meant for ground antennas and this entry
looks like a mistake, so **MRTKLIB leaves it out**.

Run on `igs20_2437.atx`, the tool output differs from the CLASLIB 0.9.1 file by
exactly that one 4-line block. With that entry added back, the output is
byte-identical.

---

## 2. ERP: `igu00p01.erp`

### 2.1 What the file is

`igu00p01.erp` is a cumulative file of IGS ultra-rapid Earth rotation parameters.
The published cumulative file **stopped at MJD 59992.25 (2023-02-17 06:00 UTC)**.
The copy on SOPAC ends there, and the CDDIS copy has the same compressed size.

The CLASLIB 0.9.0 file is that frozen file. The 0.9.1 file extends it to
MJD 61334.25 (2026-10-21 06:00 UTC) with the individual 6-hourly ultra-rapid
files, `IGS0OPSULT_YYYYDDDHHMM_02D_01D_ERP.ERP.gz`. These are the former `igu`
products under the IGS long file name. Each one holds only two rows, so the
cumulative file is an accumulation of them rather than any single file renamed.

### 2.2 The rule

- Each issue has two rows, at issue + 0.5 d and issue + 1.5 d.
- Issues are applied oldest first, and a later issue overwrites a row with the
  same MJD. A +1.5 d prediction is therefore replaced by the +0.5 d value of the
  issue one day later.
- Rows are copied verbatim, and the header of the cumulative file is kept.

The last issue in CLASLIB 0.9.1 is 2026-292 18:00. Its final four rows
(MJD 61333.50–61334.25) are still +1.5 d predictions, as expected at the end of
any such file.

### 2.3 Rows in CLASLIB 0.9.1 that do not follow the rule

Applying all 5,368 issues from 2023-02-16 00:00 to 2026-10-19 18:00 gives the
same 36,060 epochs as CLASLIB 0.9.1, and 36,055 of the rows are byte-identical.
The other five, checked on 2026-11-09 against the BKG and SOPAC mirrors (which
agree on these issues):

**MJD 60013.50 (2023-03-10 12:00 UTC): not overwritten.** CLASLIB keeps the
+1.5 d prediction from issue 2023-068 00:00. Under the rule it should be the
+0.5 d value from issue 2023-069 00:00.

| Quantity | CLASLIB 0.9.1 | Issue 2023-069 00:00 |
|:--|--:|--:|
| Xpole (10⁻⁶″) | −40623 | −40897 |
| Ypole (10⁻⁶″) | 344694 | 344812 |
| UT1−UTC (0.1 µs) | −180422 | −180800 |
| LOD (0.1 µs) | 8084 | 7884 |

**MJD 61256.25–61257.00 (2026-08-04 06:00 to 2026-08-05 00:00 UTC): values not
in any current issue.** Xpole and Ypole match the issues. UT1−UTC, LOD and their
sigmas do not, and the values are not found in any issue on the mirrors.

| MJD | UT1−UTC, CLASLIB − issue (0.1 µs) | LOD, CLASLIB − issue (0.1 µs) |
|:--|--:|--:|
| 61256.25 | −504 | +48 |
| 61256.50 | −536 | +38 |
| 61256.75 | −446 | +21 |
| 61257.00 | −440 | +25 |

The cause of these four rows is not known. Neither series is visibly smoother
across the interval, so this page does not say which one is right.

**MRTKLIB follows the rule:** `update_igu_erp.py --full` overwrites all five
rows with the values from the current issues.

### 2.4 Late issues

An issue can reach the mirrors after a later one, which is one way a row can
remain unoverwritten. A normal `update_igu_erp.py` run therefore goes back over
the last 14 days as well as fetching new issues. A missing issue that lies
before the newest applied one is reported as a warning. `--full` re-applies
every issue since 2023-02-16 (about 5,400 small files).
