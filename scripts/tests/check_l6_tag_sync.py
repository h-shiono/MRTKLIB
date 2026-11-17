#!/usr/bin/env python3
"""Check that L6 replay tags stay in step with a BINEX observation tag (#515).

Replays the ``strsync()`` timing offline: for every L6 frame it finds the
master tick at which ``mrtk run`` releases the frame (slave offset =
master tick_f - slave tick_f), then the newest BINEX epoch that is complete
in the master file at that tick.  The lag

    newest complete obs epoch - GNSS epoch of the L6 frame

is about 1 s on a live receiver (the frame is output one second after its
epoch).  Frames released before the first observation epoch is complete are
not checked: a receiver that was already running would hold them too.

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


def main():
    """Command-line entry point."""
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bnx", required=True, help="master BINEX observation file (tag = <file>.tag)")
    ap.add_argument("l6", nargs="+", help="L6 frame files (tag = <file>.tag)")
    ap.add_argument("--min-lag", type=float, default=-1.0)
    ap.add_argument("--max-lag", type=float, default=2.0)
    args = ap.parse_args()

    epochs = scan_bnx_epochs(args.bnx)  # (record start fpos, GPS seconds)
    starts = [p for p, _ in epochs]
    file_size = os.path.getsize(args.bnx)
    # epoch k is complete once the file is released up to the start of k+1
    ends = starts[1:] + [file_size]
    m_tick_f, m_tt, m_ts = read_tag_header(args.bnx + ".tag")
    m_rec = read_records(args.bnx + ".tag")
    m_ticks = [t for t, _ in m_rec]

    def newest_obs(tick):
        """GPS seconds of the newest complete epoch at master tick, or None."""
        k = bisect.bisect_right(m_ticks, tick) - 1
        if k < 0:
            return None
        released = m_rec[k][1]
        j = bisect.bisect_right(ends, released) - 1
        return epochs[j][1] if j >= 0 else None

    ok = True
    for path in args.l6:
        with open(path, "rb") as f:
            data = f.read()
        s_tick_f, _, _ = read_tag_header(path + ".tag")
        s_rec = read_records(path + ".tag")
        offset = (m_tick_f - s_tick_f + 2**31) % 2**32 - 2**31  # as strsync's (int) cast
        times = l6_frame_times(data, m_tt + m_ts)
        # first record releasing each frame
        lags = []
        k = 0
        for i, t in enumerate(times):
            end = (i + 1) * L6_FRAME_SIZE
            while k < len(s_rec) and s_rec[k][1] < end:
                k += 1
            if k == len(s_rec):
                break
            obs = newest_obs(s_rec[k][0] - offset)
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
