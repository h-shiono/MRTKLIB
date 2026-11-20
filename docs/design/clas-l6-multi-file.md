# CLAS L6 channels from several files (`mrtk post`)

**Status: implemented (#373, PR #514, v0.7.18).**
This page explains why `mrtk post` builds its CLAS channels from the file contents,
and what was decided along the way. It also lists the alternatives that were
rejected and the known limits. User-facing behaviour is described in
[CLI guide → mrtk post](../guide/cli.md#mrtk-post). The API is documented in
`include/mrtklib/mrtk_clas.h` (section "L6 File Channel Assembly").

---

## 1. Background

- Since #362, `mrtk l6extract` writes one file per L6 header PRN
  (`{prefix}.{PRN}.l6d`).
- A receiver channel tracks one QZS at a time. When it hands over, for example
  from 199 to 195, that channel's frames are split over `l6.199.l6d` and
  `l6.195.l6d`. Over a day a channel can go 199 → 195 → 199, and then one file
  holds two separate spans.
- `mrtk post` used to take one file per CLAS channel (`clas_file[CLAS_CH_NUM]`,
  at most two) and silently dropped any further file.
- Post-processing such a capture therefore meant concatenating the PRN files
  by hand, per channel and in time order. The handover tests (#372) did this
  in `scripts/tests/run_handover_post.py`, using the channel/PRN sequence
  recorded in `handover_manifest.json`.

## 2. Requirements

1. Build the channels from any number of CLAS `.l6d` / `.l6` files.
2. Line up with the real-time path: a post-processing run and an RT replay
   (`mrtk run`) of the same capture should use the same channels.
3. Without overlap in time, give exactly the result of the hand-made
   concatenation. The #372 references then double as the test oracle.
4. Leave every existing invocation unchanged, byte for byte.
5. Warn about files that are not used instead of dropping them silently.

## 3. Design

### 3.1 Channel key: the transmit pattern

- The L6 header carries the CLAS transmit-pattern ID (`l6_header_t.ptn`).
- The RT demux `clas_route_l6frame()` (#197 / #201) keys its channels on it:
  - with `l6_merge = 1`, each new pattern takes the first free channel;
  - with `l6_merge = 0`, everything goes to ch0.
- Post-processing uses the same rule (requirement 2). With `l6_merge = 1`, the
  order of first appearance is the earliest first ST1 epoch, ties going to the
  file given first.
- A file is classified by its first CLAS frame. In the handover-v1 data,
  194/196 are pattern 1 and 195/199 are pattern 0, which matches the receiver
  channels in the manifest.

### 3.2 Time source: the ST1 epoch, read without the decoder

- L6 frame files carry no time tags.
- The ST1 mask message is the first message of the subframe that starts each
  30 s CSSR cycle. Its 20-bit GNSS epoch time (TOW) sits at fixed bit
  positions: message number at frame bit 49, subtype at 61, TOW at 65.
- `clas_l6_scan_file()` reads only those bits. It synchronizes frames exactly
  as `clas_input_cssr()` does (preamble search, `L6_FRAME_LEN` bytes per frame),
  so block offsets fall on the frames the decoder sees, for both 218- and
  250-byte frame files.
- Where the code lives (an open question in #373): reading ST1 needs CSSR
  knowledge, so it is in the CLAS module (`src/clas/mrtk_clas_l6seq.c`), not in
  the generic `mrtk_l6.c`.
- A block runs from an ST1 subframe start to the next one. Frames before a
  file's first ST1 belong to its first block, so the blocks of a file add up to
  the file itself.
- An ST1 epoch that does not advance on the previous block starts no block.
- A week rollover inside a file is unwrapped. Files are aligned to the first
  usable file within half a week.

### 3.3 Reading a channel: block by block

Rule, applied at every block:

1. Take the earliest next block among the channel's files.
2. On a tie, stay on the current file.
3. Skip blocks of other files that start before the end of the last block
   read (§3.4).

Properties:

- **Switches only at ST1 boundaries.** The decoder never gets another
  satellite's frames in the middle of a subframe or a CSSR cycle.
- **No overlap → concatenation.** Each file's blocks are read in full, back to
  back (requirement 3).
- **Full overlap, the same pattern seen on two satellites:** the current file
  is kept as long as it has data. Another file is taken only where the current
  one has a gap. Like the RT lock, the reader does not flip back when the
  first file resumes.
- **A → B → A over a day** is read in that order.

The reader `clas_l6seq_input()` is the loop of `clas_input_cssrf()`, with the
byte source replaced by the segment list (`[start, end)` ranges of the input
files). Decoder state is not touched.

### 3.4 Lost ST1: never step back in time

- If the current file loses an ST1, its block runs on over the next 30 s.
- Under rule 1 alone, another file's block inside that span would be read
  next. The decoder would then see time go backwards and get duplicate
  subframes.
- The end of a block is therefore estimated as its ST1 epoch plus its frame
  count (one frame per second). Blocks of other files that start before it are
  skipped.
- A real gap still gets filled, because a block that lost frames is short.
- The same check covers the frames before a file's first ST1. They are dated
  back from that ST1, one second per frame. On a switch to the file they are
  dropped if they start before the end of the last block read, and kept
  otherwise. Keeping them where they do not overlap is what makes the
  non-overlapping case equal to concatenation.
- Unit test case (11) covers a lost ST1 (60 frames in the block) and a 2-frame
  shortfall. Case (12) covers leading frames that overlap and leading frames
  that do not.
- The lost-ST1 rule was added after the algorithm-safety review, and the
  leading-frame rule after the Greptile review of PR #514. L6 frame losses are
  common on live data (#363 / #406).

### 3.5 When the new path is used

| Input | Path |
|---|---|
| 1 CLAS file | legacy (file → ch0) |
| 2 files, different patterns, `l6_merge = 1` | legacy (file i → ch i) |
| any `%` keyword path (per-epoch file switching) | legacy; extra files are not used and warn |
| more than 2 files | grouped |
| 2 files of one pattern | grouped |
| 2 or more files with `l6_merge = 0` | grouped (all on ch0) |

- **Why `l6_merge = 0` is grouped.** On the legacy path, a second file went to
  ch1, which the engine does not read with `l6_merge = 0`.
  - For per-PRN files across a pattern change (mcla_e: 199 then 194), the file
    that starts later would have taken ch0.
  - Measured on 2025157U ch1+ch2 with `l6_merge = 0`: the grouped result is
    byte-identical to the ch1-only run. The legacy result differed by
    0.015 cm 3D RMS, because ch1 was still decoded and bootstrapped.
  - No existing test used this configuration.
- **Existing tests.** `claslib_ppp_rtk_2ch` and `post_handover_d9c_*` (two
  files, different patterns, merge on) stay on the legacy path. Every existing
  output is unchanged.

## 4. Alternatives not taken

- **File-level order.** Read each file to its end, ordered by first ST1, then
  skip what is already covered. This is simpler, but a PRN that comes back
  later in a day file (A → B → A) loses its second span.
- **Switching at any subframe.** Every `sf_ind` frame is a decoder-clean cut.
  Cutting at ST1 instead keeps a whole CSSR cycle (mask and the corrections
  that refer to it) from one satellite.
- **A temporary concatenated file.** Writing the channel to `tmpfile()` would
  have left `update_clas()` untouched. A segment list avoids the extra disk
  copy and temporary-directory issues on some platforms.
- **Grouping by file name.** L6 PRNs are not stable service identifiers (see
  the `mrtk_l6.h` notes), so file names are not used. The header pattern is
  the key the RT path already uses.

## 5. Tests

- **`utest_clas_l6seq`** (synthetic 250-byte frame files) covers:
  - scan offsets;
  - reverse input order;
  - A → B → A;
  - overlap;
  - pattern channels and their tie-break;
  - `l6_merge = 0` across patterns;
  - unusable files;
  - week rollover;
  - a one-file channel;
  - the order of the decoder input;
  - lost ST1;
  - leading frames on a switch.
- **`post_handover_{d9c_m,d9c_s,mcla_a,mcla_e,mcla_o}_prn`.** The per-PRN files
  of handover-v1 are given with each channel's PRNs in reverse time order. The
  output must be byte-identical to `post_handover_<case>`, the concatenated
  input.

## 6. Known limits and open points

- **Ties decide ch0.** When two patterns start in the same 30 s cycle (both d9c
  cases), the pattern of the file given first takes ch0.
  - The `l6_merge = 1` engine depends on channel order. On the legacy path,
    swapping d9c_m's ch0/ch1 files changes fix from 883 to 842 epochs out of
    900, and float from 5 to 40.
  - This is a property of the merge (#303), outside #373. It is a candidate
    for #394.
- **3.5-day span.** ST1 carries only the TOW. Files more than half a week
  apart are misordered, and files exactly a week apart look simultaneous.
- **Same-pattern overlap on real data** (two satellites of one pattern
  recorded at once) is covered by the synthetic tests only.
- **A file whose pattern changes part-way** (a concatenated single-channel
  capture) is classified by its first frame. This is harmless with
  `l6_merge = 0`. With `l6_merge = 1`, give per-PRN files instead.
- **Input count.** `mrtk post` takes at most 16 input files in total, and
  extra arguments are dropped by the CLI (`MAXFILE`). The CLAS assembly also
  caps at 16 (`CLAS_L6SEQ_MAXFILE`) and warns about any further file.
