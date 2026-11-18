#!/usr/bin/env python3
"""Check that L6 replay tags stay in step with a BINEX observation tag (#515).

Replays the ``strsync()`` timing offline: for every L6 frame it finds the
master tick at which ``mrtk run`` releases the frame (slave offset =
master tick_f - slave tick_f), then the newest BINEX epoch that is complete
in the master file at that tick.  The lag

    newest complete obs epoch - GNSS epoch of the L6 frame

is +1 s on a live receiver: the frame is output one second after its epoch,
an observation epoch at that epoch.  Frames output before the first
observation epoch are not checked: they are delivered together at replay
start, as a receiver that was already running would hold them.  Frames past
the last tag record are checked at tag EOF, where ``readfile()`` releases the
rest of the file.

Frame epochs come from the L6 content (CSSR ST1), observation epochs from the
BINEX file, so the check does not rely on either tag's header time.

Usage:
    check_l6_tag_sync.py --bnx OBS.bnx L6FILE [L6FILE ...] [--min-lag S] [--max-lag S]

The tags are read from ``<file>.tag``.
"""

import argparse
import bisect
import os
import statistics
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from gen_bnx_tag import scan_bnx_epochs  # noqa: E402
from gen_l6_tag import GPS_EPOCH_UNIX, L6_FRAME_SIZE, l6_frame_times, read_tag_header  # noqa: E402

TAG_HEADER_LEN = 64 + 4 + 8


def read_records(tag_path):
    """Return the (tick_n, fpos) records of a .tag file."""
    with open(tag_path, "rb") as f:
        b = f.read()
    n = (len(b) - TAG_HEADER_LEN) // 8
    return [struct.unpack_from("<II", b, TAG_HEADER_LEN + 8 * k) for k in range(n)]


class Replay:
    """File stream release timing of one tagged file, as readfile() does it.

    At tick t the file is readable up to the fpos of the first record whose
    tick is greater than t; once every record is passed (tag EOF), up to the
    end of the file.
    """

    def __init__(self, records, file_size):
        """Index ``records`` [(tick_n, fpos)] of a file of ``file_size`` bytes."""
        self.fpos = [p for _, p in records]
        self.file_size = file_size
        self.ticks_max = []  # running maximum: the first record with tick > t
        m = 0
        for t, _ in records:
            m = max(m, t)
            self.ticks_max.append(m)

    def released(self, tick):
        """Bytes readable at ``tick``."""
        k = bisect.bisect_right(self.ticks_max, tick)
        return self.fpos[k] if k < len(self.fpos) else self.file_size

    def release_tick(self, end):
        """First tick at which the file is readable up to ``end`` (None: at start)."""
        k = bisect.bisect_left(self.fpos, end)  # first record covering end
        return self.ticks_max[k - 1] if k > 0 else None


def main():
    """Command-line entry point."""
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bnx", required=True, help="master BINEX observation file (tag = <file>.tag)")
    ap.add_argument("l6", nargs="+", help="L6 frame files (tag = <file>.tag)")
    ap.add_argument("--min-lag", type=float, default=0.0)
    ap.add_argument("--max-lag", type=float, default=2.0)
    args = ap.parse_args()

    epochs = scan_bnx_epochs(args.bnx)  # (record start fpos, GPS seconds)
    file_size = os.path.getsize(args.bnx)
    # epoch k is complete once the file is readable up to the start of k+1
    ends = [p for p, _ in epochs[1:]] + [file_size]
    m_tick_f, m_tt, m_ts = read_tag_header(args.bnx + ".tag")
    master = Replay(read_records(args.bnx + ".tag"), file_size)

    def newest_obs(tick):
        """GPS seconds of the newest complete epoch at master tick, or None."""
        j = bisect.bisect_right(ends, master.released(tick)) - 1
        return epochs[j][1] if j >= 0 else None

    ok = True
    for path in args.l6:
        with open(path, "rb") as f:
            data = f.read()
        s_tick_f, _, _ = read_tag_header(path + ".tag")
        slave = Replay(read_records(path + ".tag"), len(data))
        offset = (m_tick_f - s_tick_f + 2**31) % 2**32 - 2**31  # as strsync's (int) cast
        times = l6_frame_times(data, m_tt + m_ts)
        lags = []
        for i, t in enumerate(times):
            if t - GPS_EPOCH_UNIX + 1 <= epochs[0][1]:
                continue  # output before the first observation: held at replay start
            tick = slave.release_tick((i + 1) * L6_FRAME_SIZE)
            obs = newest_obs(tick - offset) if tick is not None else None
            if obs is not None:
                lags.append(obs - (t - GPS_EPOCH_UNIX))
        if not lags:
            print(f"FAIL {path}: no frame released after the first observation epoch")
            ok = False
            continue
        lo, med, hi = min(lags), statistics.median(lags), max(lags)
        bad = sum(1 for x in lags if not (args.min_lag <= x <= args.max_lag))
        status = "OK  " if bad == 0 else "FAIL"
        print(
            f"{status} {os.path.basename(path)}: {len(lags)} frames, obs - L6 epoch lag "
            f"min {lo:+.1f} median {med:+.1f} max {hi:+.1f} s "
            f"(allowed {args.min_lag:+.1f}..{args.max_lag:+.1f}, {bad} outside)"
        )
        ok = ok and bad == 0
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
