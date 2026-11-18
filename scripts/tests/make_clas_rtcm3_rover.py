#!/usr/bin/env python3
"""Make an RTCM3 copy of a CLAS RT replay's BINEX rover (#365).

An RTCM3 rover's time advances in rtcm[0] while raw[0] stays at the start
time. The CLAS L6 paths read raw[0] only, so the L6 rate limit held L6 once it
ran 60 s ahead of the start, and the corrections aged out.

This script converts the BINEX rover to RTCM3 with ``mrtk relay`` (MSM7 for
GPS / Galileo / QZSS, plus their ephemerides), writes a ``.tag`` for it on the
timeline of the BINEX tag (same header time and ``tick_f``, one record per
message timed by the latest MSM epoch), and writes an ``mrtk run``
configuration that is ``--conf`` with the rover replaced by the RTCM3 file.

Usage:
    make_clas_rtcm3_rover.py --mrtk MRTK --bnx IN.bnx --conf IN.toml --out-dir DIR
"""

import argparse
import os
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from gen_l6_tag import GPS_EPOCH_UNIX, WEEK_SEC, _bits, read_tag_header  # noqa: E402

MSGS = "1077,1097,1117,1019,1046,1044"
# MSM1-7 of GPS / Galileo / QZSS: 30-bit epoch time = GPS TOW in ms
MSM_TYPES = set(range(1071, 1078)) | set(range(1091, 1098)) | set(range(1111, 1118))
MIN_EPOCHS = 3590  # the bundled hour has 3599 rover epochs


def relay(mrtk: str, src: Path, dst: Path, timeout: float = 120.0) -> None:
    """Run ``mrtk relay`` BINEX -> RTCM3 until the output stops growing.

    ``mrtk relay`` keeps running at the end of a file input, so it is stopped
    once the output size has not changed for 3 s.
    """
    if dst.exists():
        dst.unlink()
    cmd = [mrtk, "relay", "-in", f"file://{src}#binex", "-out", f"file://{dst}#rtcm3", "-msg", MSGS]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        last, stable, t0 = -1, 0.0, time.monotonic()
        while time.monotonic() - t0 < timeout:
            time.sleep(0.5)
            size = dst.stat().st_size if dst.exists() else 0
            stable = stable + 0.5 if size == last and size > 0 else 0.0
            last = size
            if stable >= 3.0:
                return
            if proc.poll() is not None:
                raise SystemExit(f"mrtk relay exited with {proc.returncode}")
        raise SystemExit(f"mrtk relay did not finish within {timeout:.0f} s")
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


def rtcm3_records(data: bytes, master_start: float) -> tuple:
    """Return ([(tick ms, start offset)], MSM count): each message is released at
    its MSM epoch (other messages at the latest one), as (epoch - master start)
    on the master timeline. readfile() makes the file readable up to the offset
    of the first record whose tick is still ahead, so a record carries the start
    of its message (as in gen_l6_tag.py); a last record releases the tail."""
    week = int((master_start - GPS_EPOCH_UNIX) // WEEK_SEC)
    recs, i, tick, nmsm = [], 0, 0, 0
    while i + 6 <= len(data):
        if data[i] != 0xD3:
            i += 1
            continue
        n = ((data[i + 1] & 3) << 8) | data[i + 2]
        if i + 6 + n > len(data):
            break
        payload = data[i + 3 : i + 3 + n]
        if _bits(payload, 0, 12) in MSM_TYPES:
            gpst = GPS_EPOCH_UNIX + week * WEEK_SEC + _bits(payload, 24, 30) / 1000.0
            if gpst - master_start < -WEEK_SEC / 2:
                gpst += WEEK_SEC
            tick = max(tick, round((gpst - master_start) * 1000.0))
            nmsm += 1
        recs.append((tick, i))
        i += 6 + n
    recs.append((tick + 1000, i))
    return recs, nmsm


def write_tag(path: Path, master_tag: Path, recs: list) -> None:
    """Write ``path`` with the master tag's header time and ``tick_f``."""
    tick_f, time_time, time_sec = read_tag_header(str(master_tag))
    hdr = bytearray(64)
    label = b"TIMETAG MRTKLIB make_clas_rtcm3_rover"
    hdr[: len(label)] = label
    struct.pack_into("<I", hdr, 60, tick_f)
    with open(path, "wb") as fp:
        fp.write(hdr)
        fp.write(struct.pack("<Id", time_time, time_sec))
        for tick, pos in recs:
            fp.write(struct.pack("<II", tick, pos))


def main() -> int:
    """CLI entry point."""
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--mrtk", required=True)
    ap.add_argument("--bnx", required=True, type=Path)
    ap.add_argument("--conf", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    out = args.out_dir / (args.bnx.stem + ".rtcm3")
    master_tag = Path(str(args.bnx) + ".tag")

    relay(args.mrtk, args.bnx, out)
    _, time_time, time_sec = read_tag_header(str(master_tag))
    recs, nmsm = rtcm3_records(out.read_bytes(), time_time + time_sec)
    nepoch = len({t for t, _ in recs})
    if nepoch < MIN_EPOCHS:
        print(
            f"ERROR: {out}: only {nepoch} rover epochs (expected >= {MIN_EPOCHS})", file=sys.stderr
        )
        return 1
    write_tag(Path(str(out) + ".tag"), master_tag, recs)

    conf = args.conf.read_text()
    rover = f'path = "./tests/data/claslib/{args.bnx.name}::T::x10"'
    fmt = 'format = "binex"'
    if conf.count(rover) != 1 or conf.count(fmt) != 1:
        print(f"ERROR: BINEX rover stream not found once in {args.conf}", file=sys.stderr)
        return 1
    conf = conf.replace(rover, f'path = "{out}::T::x10"').replace(fmt, 'format = "rtcm3"')
    out_conf = args.out_dir / "rtkrcv_clas_rtcm3.toml"
    out_conf.write_text(conf)

    print(
        f"wrote {out} ({out.stat().st_size} bytes, {nmsm} MSM, {nepoch} epochs), its .tag and {out_conf}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
