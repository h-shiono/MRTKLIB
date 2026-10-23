#!/usr/bin/env python3
"""Build the QZSS L6 handover regression dataset (#372).

Slices five ~15-minute windows around CLAS L6 satellite handovers from the
maintainer's DOY 2026-286 captures, synthesises file-stream time tags,
checks that no receiver channel interleaves two L6 PRNs, and writes
``handover_manifest.json`` plus ``tests/data/handover/README.md``. The slices
are packed into the release asset ``handover-v<N>.tar.gz`` (#386) with
``make_dataset_asset.py``; nothing large is committed.

Cases (window = handover - 600 s .. handover + 300 s); slice files keep the
capture name plus the window length (G5P3286m.sbf -> G5P3286m_15m.sbf, the
MCLA0286* capture-name typo fixed to MCLA286*):

    d9c_m   NEO-D9C UBX (L6 only, 2 ch) + mosaic-G5 SBF observations
    d9c_s   same receivers, later hour
    mcla_a  mosaic-CLAS SBF (observations + L6 in one stream, 1 ch)
    mcla_e  mosaic-CLAS, transmit pattern 1 -> 2
    mcla_o  mosaic-CLAS, transmit pattern 2 -> 1

Time tags (``<file>.tag``, read by ``mrtk run`` file streams with ``::T``) use
one common axis, GPST ms of week: the header tick is the file's first record,
each record is (ms since that tick, file offset after the record). Two streams
replayed together are synchronised by the header-tick difference
(``syncfile()``). SBF records take the block TOW; UBX records take the iTOW of
the latest NAV-PVT (RXM-QZSSL6 ``timeTag`` is not GPST).

Usage (from the repository root; captures in CAPTURE_DIR):
    make_handover_fixtures.py CAPTURE_DIR OUTDIR [--version N]
"""

import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_dataset_asset  # noqa: E402
import sbf_filter  # noqa: E402
import ubx_filter  # noqa: E402

BEFORE_S, AFTER_S = 600, 300
GPS_EPOCH_UNIX = 315964800  # 1980-01-06 00:00:00, gtime_t seconds of GPST week 0
TAG_HEADER = b"TIMETAG MRTKLIB (synthetic, scripts/tests/make_handover_fixtures.py)"

# Observation + navigation blocks used by the GPS/Galileo/QZSS CLAS configs:
# MeasEpoch, PVTGeodetic, GPS C/A, Galileo F/NAV + I/NAV, QZSS C/A, and the
# decoded GPS/Galileo/QZSS ephemerides. MeasExtra (4000) is not decoded,
# BDS/GLONASS/SBAS are not used, and the G5's own L6 (4270/4271) must stay out
# of the D9C cases (it would reach the CLAS demux next to the D9C frames).
OBS_NAV_BLOCKS = "4027,4007,4017,4022,4023,4066,5891,4002,4095"

# (case, handover TOW [s], L6 source file, obs SBF, description). D9C
# handover TOWs are NAV-PVT iTOW; MCLA are SBF TOW. MCLA0286* is a typo in
# the capture names for MCLA286*.
CASES = [
    ("d9c_m", 218778.5, "D9C1286m.ubx", "G5P3286m.sbf", "D9C ch1 199->195 (pattern 1), 2 ch"),
    ("d9c_s", 240903.5, "D9C1286s.ubx", "G5P3286s.sbf", "D9C ch0 194->196 (pattern 2), 2 ch"),
    ("mcla_a", 176074.0, "MCLA0286a.sbf", None, "MCLA 195->199, 1 ch"),
    ("mcla_e", 190366.0, "MCLA0286e.sbf", None, "MCLA 199->194, pattern 1->2, 1 ch"),
    ("mcla_o", 225943.0, "MCLA0286o.sbf", None, "MCLA 194->195, pattern 2->1, 1 ch"),
]


def slice_name(src):
    """Slice file name: capture name (station + DOY + hour; the MCLA0286*
    typo fixed) + window length, e.g. G5P3286m.sbf -> G5P3286m_15m.sbf."""
    stem, ext = os.path.splitext(src)
    return f"{stem.replace('MCLA0', 'MCLA')}_{(BEFORE_S + AFTER_S) // 60}m{ext}"


def write_tag(path, records, week):
    """Write a file-stream time tag: header tick = first record (GPST ms of
    week), records = (ms since header tick, end offset), ticks monotonic."""
    t0 = records[0][0]
    tow0 = t0 / 1000.0
    head = TAG_HEADER[:60].ljust(60, b"\0") + struct.pack("<I", t0)
    out = bytearray(head)
    out += struct.pack("<I", GPS_EPOCH_UNIX + week * 604800 + int(tow0))
    out += struct.pack("<d", tow0 - int(tow0))
    last = 0
    for tow, pos in records:
        last = max(last, tow - t0)
        out += struct.pack("<II", last, pos)
    with open(path, "wb") as fp:
        fp.write(out)
    return t0


def sbf_l6_prns(data):
    """[(tow_ms, header PRN)] for SBF QZSRawL6 (4069) blocks with CRC/parity OK."""
    out, i, n = [], 0, len(data)
    while True:
        i = data.find(b"$@", i)
        if i < 0 or i + 8 > n:
            return out
        bid = struct.unpack_from("<H", data, i + 4)[0] & 0x1FFF
        length = struct.unpack_from("<H", data, i + 6)[0]
        if length < 8 or length % 4 or i + length > n:
            i += 1
            continue
        if bid == 4069 and length >= 272 and data[i + 15]:
            out.append((struct.unpack_from("<I", data, i + 8)[0], data[i + 27]))  # frame byte 4
        i += length


def ubx_l6_prns(data):
    """{channel: [(itow_ms, header PRN)]} for error-free RXM-QZSSL6."""
    out = {}
    for itow, cls, mid, msg in ubx_filter.timed_messages(data):
        if (cls, mid) != ubx_filter.RXM_QZSSL6 or len(msg) < 8 + 264:
            continue
        info = struct.unpack_from("<H", msg, 6 + 10)[0]
        if (info >> 12) & 3 != 1:
            continue
        out.setdefault((info >> 8) & 3, []).append((itow, msg[6 + 14 + 4]))
    return out


def runs(seq):
    """Collapse [(t, prn)] into [(prn, first_t, last_t, count)]; a PRN that
    reappears after another one means the channel interleaves."""
    out = []
    for t, prn in seq:
        if out and out[-1][0] == prn:
            p, t0, _, k = out[-1]
            out[-1] = (p, t0, t, k + 1)
        else:
            out.append((prn, t, t, 1))
    prns = [r[0] for r in out]
    if len(prns) != len(set(prns)):
        raise SystemExit(f"channel interleaves L6 PRNs: {prns}")
    return out


def describe(channels):
    return {
        str(ch): [{"prn": p, "first_tow_ms": a, "last_tow_ms": b, "frames": k} for p, a, b, k in r]
        for ch, r in sorted(channels.items())
    }


def build_case(case, h, l6src, obssrc, desc, capdir, outdir):
    start = int(h) * 1000 - BEFORE_S * 1000
    end = int(h) * 1000 + AFTER_S * 1000
    entry = {
        "case": case,
        "description": desc,
        "handover_tow": h,
        "window_tow_ms": [start, end],
        "files": {},
    }

    def add(name, data, records, week, source, kept, l6):
        path = os.path.join(outdir, name)
        with open(path, "wb") as fp:
            fp.write(data)
        t0 = write_tag(path + ".tag", records, week)
        entry["files"][name] = {
            "source": source,
            "bytes": len(data),
            "tag_start_tow_ms": t0,
            "kept": {str(k): v for k, v in sorted(kept.items())},
            "l6": l6,
        }

    if l6src.endswith(".ubx"):  # D9C: UBX L6 + G5 SBF observations
        with open(os.path.join(capdir, obssrc), "rb") as fp:
            raw = fp.read()
        obs, st = sbf_filter.filter_sbf(raw, sbf_filter.parse_blocks(OBS_NAV_BLOCKS), start, end)
        week = st["wnc"]
        add(slice_name(obssrc), obs, st["records"], week, obssrc, st["kept"], False)
        with open(os.path.join(capdir, l6src), "rb") as fp:
            raw = fp.read()
        l6, rec, kept = ubx_filter.filter_ubx(
            raw, ubx_filter.parse_msgs(ubx_filter.DEFAULT_MSGS), start, end
        )
        add(
            slice_name(l6src),
            l6,
            rec,
            week,
            l6src,
            {f"{c:02x}-{m:02x}": k for (c, m), k in kept.items()},
            True,
        )
        channels = {ch: runs(seq) for ch, seq in ubx_l6_prns(l6).items()}
    else:  # MCLA: one SBF stream with observations and L6
        with open(os.path.join(capdir, l6src), "rb") as fp:
            raw = fp.read()
        blocks = sbf_filter.parse_blocks(OBS_NAV_BLOCKS + ",4069")
        data, st = sbf_filter.filter_sbf(raw, blocks, start, end)
        add(
            slice_name(l6src),
            data,
            st["records"],
            st["wnc"],
            l6src.replace("MCLA0", "MCLA"),
            st["kept"],
            True,
        )
        channels = {0: runs(sbf_l6_prns(data))}

    entry["l6_channels"] = describe(channels)
    switched = [r for r in channels.values() if len(r) > 1]
    if len(switched) != 1 or len(switched[0]) != 2:
        raise SystemExit(f"{case}: expected exactly one channel with one handover, got {channels}")
    entry["measured_handover_tow_ms"] = switched[0][1][1]
    return entry


def write_readme(path, manifest, asset, sha):
    lines = [
        "# QZSS L6 handover regression dataset",
        "",
        f"Release asset `{os.path.basename(asset)}` of",
        "[h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data) (#386), used by the",
        "handover regression tests (#372). Built by `scripts/tests/make_handover_fixtures.py`;",
        "nothing here is committed except this README.",
        "",
        f"SHA256 `{sha}`",
        "",
        "Captures: maintainer recordings of DOY 2026-286 (2026-10-13), one antenna",
        "(`TRM55971.00     NONE`, the ZED-F9P ECJ02 point) split to a mosaic-G5 (G5P3),",
        "a NEO-D9C (D9C1, L6 only) and a mosaic-CLAS (MCLA; the capture files are",
        "misnamed `MCLA0286*`). Each window is handover - 600 s .. + 300 s.",
        "",
        f"SBF blocks kept for observations/navigation: {OBS_NAV_BLOCKS}",
        "(MeasEpoch, PVTGeodetic, GPS C/A, Galileo F/NAV + I/NAV, QZSS C/A, decoded",
        "GPS/Galileo/QZSS ephemerides); MCLA also keeps 4069 (QZSRawL6). D9C keeps",
        "UBX NAV-PVT + RXM-QZSSL6. Every file has a synthetic `.tag` on a common",
        "GPST axis (see the script docstring).",
        "",
        "| Case | Files | Source | Window (TOW s) | Bytes | L6 channels (PRN: frames, first..last TOW) |",
        "|---|---|---|---|---|---|",
    ]
    for e in manifest["cases"]:
        w = e["window_tow_ms"]
        ch = "; ".join(
            f"ch{c}: "
            + " → ".join(
                f"{r['prn']}: {r['frames']}, {r['first_tow_ms'] / 1000:.1f}..{r['last_tow_ms'] / 1000:.1f}"
                for r in rs
            )
            for c, rs in e["l6_channels"].items()
        )
        for name, f in e["files"].items():
            ch_col = ch if f["l6"] else "—"
            lines.append(
                f"| {e['case']} | `{name}` | {f['source']} | {w[0] / 1000:.0f}–{w[1] / 1000:.0f} | {f['bytes']} | {ch_col} |"
            )
    lines += [
        "",
        "Purpose: RT replay (`mrtk run`) and post-processing (`mrtk convert` +",
        "`mrtk l6extract` + per-channel concatenation + `mrtk post`) through a CLAS",
        "L6 handover: recovery to fix, no solution freeze (#205/#218), satellite count.",
        "`handover_manifest.json` (in the asset) holds the same data for the tests.",
        "",
    ]
    with open(path, "w") as fp:
        fp.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("capdir", help="directory with the DOY 286 captures")
    ap.add_argument("outdir", help="output directory for slices, tags and the asset")
    ap.add_argument("--version", type=int, default=1)
    ap.add_argument("--readme", default="tests/data/handover/README.md")
    args = ap.parse_args()

    slices = os.path.join(args.outdir, "slices")
    os.makedirs(slices, exist_ok=True)
    manifest = {"dataset": f"handover-v{args.version}", "cases": []}
    for case in CASES:
        manifest["cases"].append(build_case(*case, args.capdir, slices))
        e = manifest["cases"][-1]
        print(
            f"{e['case']}: handover measured {e['measured_handover_tow_ms'] / 1000:.1f} (nominal {e['handover_tow']})"
        )
    with open(os.path.join(slices, "handover_manifest.json"), "w") as fp:
        json.dump(manifest, fp, indent=1)

    files = sorted(os.path.join(slices, f) for f in os.listdir(slices))
    asset = make_dataset_asset.build(manifest["dataset"], args.outdir, files)
    with open(asset, "rb") as fp:
        import hashlib

        sha = hashlib.sha256(fp.read()).hexdigest()
    os.makedirs(os.path.dirname(args.readme), exist_ok=True)
    write_readme(args.readme, manifest, asset, sha)
    print(f"asset {asset} ({os.path.getsize(asset)} bytes) sha256 {sha}")
    print(f"mrtk_dataset({manifest['dataset']} SHA256 {sha})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
