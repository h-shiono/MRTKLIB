#!/usr/bin/env python3
"""Independent "golden" decode of BeiDou PPP-B2b corrections via cssrlib.

Produces a JSONL reference dump for cross-validating the MRTKLIB C PPP-B2b
decoder. Read-only with respect to the MRTKLIB repo except for the output
file. Drives the cssrlib reference implementation (cssr_bds) directly so the
per-message-type CSSR decode is all cssrlib's own code.

cssrlib is an external run-time dependency of this script only (not bundled
with, linked into, or copied into MRTKLIB): MIT License, Copyright (c) 2021
Rui Hirokawa, https://github.com/hirokawa/cssrlib.

Input format (233h_bdsb2b.txt, one message per line):
    week tow prn type len hexpayload
The 64-byte hexpayload carries the 486-bit B2b message MSB-first
(MesType(6) + data(456) + CRC-24Q(24)). The CRC is RTCM CRC-24Q (poly
0x1864CFB) over the first 462 bits; this script CRC-checks every message
and skips failures and MT63 null messages, then feeds the surviving 64-byte
buffer to cssr_bds.decode_cssr(msg, 0) in time order.

cssr_bds initialization mirrors the cssrlib-data sample test_pppbds.py:
    cs.week = <GPS week of record>
    cs.tow0 = <GPS tow> // 86400 * 86400   (start of GPS day, seconds)
tow0 is set per message from the record's own TOW so the 17-bit BDS
seconds-of-day in each header resolves to an absolute time.

Scaling / sign conventions emitted (all cssrlib-derived, raw decoded values):
  MT2 dorb[3]   orbit dx/dy/dz [m], scale [0.0016, 0.0064, 0.0064]
  MT3 dcb       code bias       [m], scale 0.017, 12-bit
  MT4/MT6/MT7 dclk  clock correction [m], scale 0.0016, 15-bit
  iodn          MT2 raw 10-bit IODN; for GPS/BDS cssrlib also stores iodn&0xff
The B2b clock correction sign: cssrlib STORES dclk negated relative to the
raw scaled bitfield (see cssr_bds.decode_cssr_clk_sat: "the sign of the clock
correction reversed"). Both values are emitted per clock entry:
  dclk_raw    = sval(bits) * 0.0016        (raw scaled bitfield, ICD sign)
  dclk_stored = -dclk_raw                  (what cssrlib keeps in lc.dclk)
A C decoder must replicate whichever convention it consumes downstream.

"data not available" sval sentinels (u == -2**(n-1) or -(2**(n-1)-1)) decode
to NaN and are emitted as null.

Usage:
    python3 scripts/tests/b2b_cssrlib_decode.py \
        --txt 233h_bdsb2b.txt --out 233h_b2b_cssrlib.jsonl
"""

import argparse
import json
import math
import os
import sys
from binascii import unhexlify
from collections import Counter

CRC24Q_POLY = 0x1864CFB


def _import_cssrlib():
    """Import cssrlib symbols (deferred so --help works without it)."""
    try:
        import cssrlib  # noqa: F401
    except ImportError:
        src = os.environ.get("CSSRLIB_SRC")
        if src and src not in sys.path:
            sys.path.insert(0, src)
        try:
            import cssrlib  # noqa: F401
        except ImportError:
            sys.exit(
                "error: cssrlib is required but not importable.\n"
                "  Install it (pip install cssrlib) or point CSSRLIB_SRC at a\n"
                "  cssrlib source checkout, e.g.:\n"
                "    CSSRLIB_SRC=/path/to/cssrlib/src python3 "
                "scripts/tests/b2b_cssrlib_decode.py ...\n"
                "  See https://github.com/hirokawa/cssrlib"
            )

    from cssrlib.cssr_bds import cssr_bds
    from cssrlib.cssrlib import sCType
    from cssrlib.gnss import sat2id, sat2prn, uGNSS

    return cssr_bds, sCType, sat2id, sat2prn, uGNSS


def crc24q_bits(payload, nbits):
    """Bit-serial RTCM CRC-24Q over the first ``nbits`` bits of ``payload``."""
    crc = 0
    for k in range(nbits):
        bit = (payload[k >> 3] >> (7 - (k & 7))) & 1
        crc ^= bit << 23
        crc <<= 1
        if crc & 0x1000000:
            crc ^= CRC24Q_POLY
    return crc & 0xFFFFFF


def rx_crc24(payload):
    """Read the transmitted CRC: bits 462..485 of the MSB-first message."""
    val = 0
    for k in range(462, 486):
        bit = (payload[k >> 3] >> (7 - (k & 7))) & 1
        val = (val << 1) | bit
    return val


def _jval(x):
    """NaN -> None, numpy scalar -> python scalar, for JSON."""
    if x is None:
        return None
    if hasattr(x, "item"):
        x = x.item()
    if isinstance(x, float) and math.isnan(x):
        return None
    return x


def main():
    ap = argparse.ArgumentParser(description="Golden BDS PPP-B2b decode via cssrlib.")
    ap.add_argument("--txt", default="233h_bdsb2b.txt", help="input text dump")
    ap.add_argument("--out", default="233h_b2b_cssrlib.jsonl", help="output JSONL")
    args = ap.parse_args()

    cssr_bds, sCType, sat2id, sat2prn, uGNSS = _import_cssrlib()

    cs = cssr_bds()
    cs.monlevel = 0

    out_f = open(args.out, "w")
    header = {
        "format": "golden BDS PPP-B2b decode via cssrlib",
        "values": "raw cssrlib-decoded values",
        "dorb": "MT2 orbit dx/dy/dz [m], scale [0.0016, 0.0064, 0.0064]",
        "dclk_raw": "clock corr [m] = sval(bits)*0.0016 (raw ICD sign)",
        "dclk_stored": "= -dclk_raw, the value cssrlib keeps in lc.dclk",
        "dcb": "MT3 code bias [m], scale 0.017, 12-bit",
        "iodn": "MT2 raw 10-bit IODN; iodn_e = iodn&0xff for GPS/BDS",
        "ura": "URA value [m] from quality_idx(class,val); null = undefined",
        "null": "data-not-available sentinel (NaN)",
    }
    out_f.write("# " + json.dumps(header) + "\n")

    mt_hist = Counter()
    prns = set()
    tows = []
    n_pass = n_fail = n_null = 0
    n_lines = 0
    n_iod_mismatch = 0
    mask_summary = []
    quirks = Counter()
    week = None

    def sys_name(sysv):
        return {
            uGNSS.GPS: "GPS",
            uGNSS.GAL: "GAL",
            uGNSS.GLO: "GLO",
            uGNSS.BDS: "BDS",
        }.get(sysv, str(sysv))

    for line in open(args.txt):
        parts = line.split()
        if len(parts) < 6:
            continue
        week = int(parts[0])
        tow = int(parts[1])
        prn = int(parts[2])
        payload = unhexlify(parts[5])
        if len(payload) != 64:
            continue

        if crc24q_bits(payload, 462) != rx_crc24(payload):
            n_fail += 1
            continue
        n_pass += 1

        mt = payload[0] >> 2
        if mt == 63:
            n_null += 1
            continue

        mt_hist[mt] += 1
        prns.add(prn)
        tows.append(tow)

        cs.week = week
        cs.tow0 = tow // 86400 * 86400

        # snapshot state needed to compute per-message deltas before decode
        sat_n_before = list(cs.sat_n)
        iodp_before = cs.iodp
        ret = cs.decode_cssr(payload, 0)

        # cssrlib returns -1 from a block decoder when the message's IOD SSR
        # does not match the currently loaded mask; count and skip these.
        if ret == -1:
            n_iod_mismatch += 1
            quirks["iod_ssr_mismatch_skipped"] += 1
            continue

        rec = {
            "week": week,
            "tow": tow,
            "prn": prn,
            "mt": mt,
            "tod": _jval(cs.tod) if hasattr(cs, "tod") else None,
            "iodssr": _jval(cs.iodssr),
        }
        lc = cs.lc[0]

        if mt == 1:
            # MT1 mask: per-system satellite lists
            sys_lists = {}
            for sat in cs.sat_n:
                sysv, p = sat2prn(sat)
                sys_lists.setdefault(sys_name(sysv), []).append(p)
            rec["iodp"] = _jval(cs.iodp)
            rec["iodssr"] = _jval(cs.iodssr)
            rec["mask"] = {k: sorted(v) for k, v in sys_lists.items()}
            rec["nsat"] = len(cs.sat_n)
            mask_summary.append(
                {
                    "tow": tow,
                    "iodp": int(cs.iodp),
                    "counts": {k: len(v) for k, v in sys_lists.items()},
                }
            )
            if iodp_before != cs.iodp and iodp_before != -1:
                quirks["iodp_transition"] += 1

        elif mt == 2:
            # MT2 orbit + URA: emit the entries written this message. cssrlib
            # accumulates into lc.dorb/iode/iodc and self.ura keyed by sat; we
            # re-walk the raw bitfields to know which 6 slots this message
            # carried (independent of accumulation).
            rec["entries"] = _decode_orb_entries(cs, payload, sat_n_before, sat2prn, sys_name)

        elif mt == 3:
            rec["entries"] = _decode_cbias_entries(cs, payload, sat2prn, sys_name)

        elif mt == 4:
            rec.update(_decode_clk(cs, payload))

        elif mt == 5:
            rec["entries"] = _decode_ura(cs, payload)

        elif mt == 6:
            rec.update(_decode_comb1(cs, payload, sat_n_before, sat2prn, sys_name))

        elif mt == 7:
            rec.update(_decode_comb2(cs, payload, sat_n_before, sat2prn, sys_name))

        out_f.write(json.dumps(rec) + "\n")
        n_lines += 1

    out_f.close()

    # ---- report ----
    print(f"input: {args.txt}")
    print(f"CRC pass: {n_pass}  CRC fail: {n_fail}  MT63 null skipped: {n_null}")
    print(f"MT histogram (decoded): {dict(sorted(mt_hist.items()))}")
    if tows:
        print(f"week: {week}  tow range: {min(tows)} .. {max(tows)}")
    print(f"distinct PRNs: {sorted(prns)}")
    print(f"JSONL lines written: {n_lines}")
    print(f"IOD-SSR mismatch (cssrlib ret -1, skipped): {n_iod_mismatch}")
    if mask_summary:
        iodps = sorted({m["iodp"] for m in mask_summary})
        print(f"MT1 masks seen: {len(mask_summary)}  IODP value(s): {iodps}")
        print(f"  first mask counts: {mask_summary[0]['counts']}")
        print(f"  last  mask counts: {mask_summary[-1]['counts']}")
    print(f"quirks: {dict(quirks)}")
    print(f"output: {os.path.abspath(args.out)}")


def _bits(payload, pos, n):
    """Unsigned big-endian field of ``n`` bits starting at bit ``pos``."""
    v = 0
    for k in range(n):
        bit = (payload[(pos + k) >> 3] >> (7 - ((pos + k) & 7))) & 1
        v = (v << 1) | bit
    return v, pos + n


def _sbits(payload, pos, n):
    """Signed (two's-complement) big-endian field of ``n`` bits."""
    v, pos = _bits(payload, pos, n)
    if v & (1 << (n - 1)):
        v -= 1 << n
    return v, pos


def _decode_orb_entries(cs, payload, sat_n_before, sat2prn, sys_name):
    """Re-walk MT2 to report the 6 per-sat orbit slots (cssrlib scaling)."""
    # MT2 layout after MesType(6): head = tod(17) res(4) iodssr(2) = 23 bits,
    # then 6 * orb_sat. orb_sat = slot(9) iodn(10) iodc(3) dx dy dz ucls(3) uval(3).
    pos = 6 + 23
    entries = []
    sat_n = set(sat_n_before)
    for _ in range(6):
        slot, pos = _bits(payload, pos, 9)
        iodn, pos = _bits(payload, pos, 10)
        iodc, pos = _bits(payload, pos, 3)
        dx, pos = _sbits(payload, pos, cs.dorb_blen[0])
        dy, pos = _sbits(payload, pos, cs.dorb_blen[1])
        dz, pos = _sbits(payload, pos, cs.dorb_blen[2])
        ucls, pos = _bits(payload, pos, 3)
        uval, pos = _bits(payload, pos, 6 - 3)
        sysv, prn = cs.slot2prn(slot)
        if sysv == 0:  # uGNSS.NONE
            continue
        iodn_e = (iodn & 0xFF) if sys_name(sysv) in ("GPS", "BDS") else iodn
        dorb = [
            _jval(cs.sval(dx, cs.dorb_blen[0], cs.dorb_scl[0])),
            _jval(cs.sval(dy, cs.dorb_blen[1], cs.dorb_scl[1])),
            _jval(cs.sval(dz, cs.dorb_blen[2], cs.dorb_scl[2])),
        ]
        entries.append(
            {
                "slot": slot,
                "sys": sys_name(sysv),
                "prn": prn,
                "iodn": iodn,
                "iodn_e": iodn_e,
                "iod_corr": iodc,
                "dorb": dorb,
                "ura_cls": ucls,
                "ura_val": uval,
                "ura": _jval(cs.quality_idx(ucls, uval)),
            }
        )
    return entries


def _decode_cbias_entries(cs, payload, sat2prn, sys_name):
    """Re-walk MT3 code-bias entries (cssrlib scaling)."""
    pos = 6 + 23
    nsat, pos = _bits(payload, pos, 5)
    entries = []
    for _ in range(nsat):
        slot, pos = _bits(payload, pos, 9)
        nsig, pos = _bits(payload, pos, 4)
        sysv, prn = cs.slot2prn(slot)
        sigs = []
        for _ in range(nsig):
            sig, pos = _bits(payload, pos, 4)
            cb, pos = _sbits(payload, pos, cs.cb_blen)
            sigs.append({"mode": sig, "dcb": _jval(cs.sval(cb, cs.cb_blen, cs.cb_scl))})
        entries.append({"slot": slot, "sys": sys_name(sysv), "prn": prn, "sigs": sigs})
    return entries


def _decode_clk(cs, payload):
    """Re-walk MT4 clock corrections (index-addressed via iodp+st1)."""
    pos = 6 + 23
    iodp, pos = _bits(payload, pos, 4)
    st1, pos = _bits(payload, pos, 5)
    out = {"iodp": iodp, "subtype1": st1}
    if iodp != cs.iodp:
        out["entries"] = []
        out["iodp_mismatch"] = True
        return out
    entries = []
    for k in range(23):
        idx = st1 * 23 + k
        iodc, pos = _bits(payload, pos, 3)
        dclk, pos = _sbits(payload, pos, cs.dclk_blen)
        if idx < cs.nsat_n:
            raw = cs.sval(dclk, cs.dclk_blen, cs.dclk_scl)
            entries.append(
                {
                    "idx": idx,
                    "iod_corr": iodc,
                    "dclk_raw": _jval(raw),
                    "dclk_stored": _jval(-raw if not _isnan(raw) else raw),
                }
            )
    out["entries"] = entries
    return out


def _decode_ura(cs, payload):
    """Re-walk MT5 URA entries (index-addressed via iodp+st2)."""
    pos = 6 + 23
    iodp, pos = _bits(payload, pos, 4)
    st2, pos = _bits(payload, pos, 3)
    entries = []
    for k in range(70):
        ucls, pos = _bits(payload, pos, 3)
        uval, pos = _bits(payload, pos, 3)
        idx = st2 * 70 + k
        if idx < cs.nsat_n:
            entries.append(
                {
                    "idx": idx,
                    "ura_cls": ucls,
                    "ura_val": uval,
                    "ura": _jval(cs.quality_idx(ucls, uval)),
                }
            )
    return entries


def _decode_comb1(cs, payload, sat_n_before, sat2prn, sys_name):
    """Re-walk MT6: numc clock entries (slot-start addressed) + numo orbits."""
    pos = 6
    numc, pos = _bits(payload, pos, 5)
    numo, pos = _bits(payload, pos, 3)
    out = {"numc": numc, "numo": numo, "clk": [], "orb": []}
    if numc > 0:
        # tod(17) res(4) iodssr(2) iodp(4) slot_s(9) = 36 bits
        pos += 17 + 4 + 2
        iodp, pos = _bits(payload, pos, 4)
        slot_s, pos = _bits(payload, pos, 9)
        for k in range(numc):
            iodc, pos = _bits(payload, pos, 3)
            dclk, pos = _sbits(payload, pos, cs.dclk_blen)
            raw = cs.sval(dclk, cs.dclk_blen, cs.dclk_scl)
            out["clk"].append(
                {
                    "idx": slot_s + k,
                    "iod_corr": iodc,
                    "dclk_raw": _jval(raw),
                    "dclk_stored": _jval(-raw if not _isnan(raw) else raw),
                }
            )
    if numo > 0:
        pos += 17 + 4 + 2
        for _ in range(numo):
            pos = _append_orb(cs, payload, pos, out["orb"], sys_name)
    return out


def _decode_comb2(cs, payload, sat_n_before, sat2prn, sys_name):
    """Re-walk MT7: numc clock entries (per-entry slot) + numo orbits."""
    pos = 6
    numc, pos = _bits(payload, pos, 5)
    numo, pos = _bits(payload, pos, 3)
    out = {"numc": numc, "numo": numo, "clk": [], "orb": []}
    if numc > 0:
        pos += 17 + 4 + 2
        for _ in range(numc):
            slot, pos = _bits(payload, pos, 9)
            iodc, pos = _bits(payload, pos, 3)
            dclk, pos = _sbits(payload, pos, cs.dclk_blen)
            sysv, prn = cs.slot2prn(slot)
            raw = cs.sval(dclk, cs.dclk_blen, cs.dclk_scl)
            out["clk"].append(
                {
                    "slot": slot,
                    "sys": sys_name(sysv),
                    "prn": prn,
                    "iod_corr": iodc,
                    "dclk_raw": _jval(raw),
                    "dclk_stored": _jval(-raw if not _isnan(raw) else raw),
                }
            )
    if numo > 0:
        pos += 17 + 4 + 2
        for _ in range(numo):
            pos = _append_orb(cs, payload, pos, out["orb"], sys_name)
    return out


def _append_orb(cs, payload, pos, lst, sys_name):
    slot, pos = _bits(payload, pos, 9)
    iodn, pos = _bits(payload, pos, 10)
    iodc, pos = _bits(payload, pos, 3)
    dx, pos = _sbits(payload, pos, cs.dorb_blen[0])
    dy, pos = _sbits(payload, pos, cs.dorb_blen[1])
    dz, pos = _sbits(payload, pos, cs.dorb_blen[2])
    ucls, pos = _bits(payload, pos, 3)
    uval, pos = _bits(payload, pos, 3)
    sysv, prn = cs.slot2prn(slot)
    iodn_e = (iodn & 0xFF) if sys_name(sysv) in ("GPS", "BDS") else iodn
    lst.append(
        {
            "slot": slot,
            "sys": sys_name(sysv),
            "prn": prn,
            "iodn": iodn,
            "iodn_e": iodn_e,
            "iod_corr": iodc,
            "dorb": [
                _jval(cs.sval(dx, cs.dorb_blen[0], cs.dorb_scl[0])),
                _jval(cs.sval(dy, cs.dorb_blen[1], cs.dorb_scl[1])),
                _jval(cs.sval(dz, cs.dorb_blen[2], cs.dorb_scl[2])),
            ],
            "ura_cls": ucls,
            "ura_val": uval,
            "ura": _jval(cs.quality_idx(ucls, uval)),
        }
    )
    return pos


def _isnan(x):
    return isinstance(x, float) and math.isnan(x)


if __name__ == "__main__":
    main()
