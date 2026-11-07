#!/usr/bin/env python3
"""Build the SouthPAN PVS in-area dataset (southpan-v<N>, #418).

Two one-hour cases inside the PVS service area, from cssrlib-data (MIT):

  ALBY 2025-233 hour h  PolaRx5 observations at Albany (AU) + the PRN 122
                        L5 messages logged by a mosaic-X5 in Kamakura (SIS)
  ALIC 2025-110 hour f  observations at Alice Springs (AU) + a SouthPAN Data
                        Access Service log (SISNeT *MSG lines)

and the IGS merged broadcast navigation of each day
(BRD400DLR_S_<yyyy><doy>0000_01D_MN.rnx, DLR). cssrlib-data's own
alby233h_rnx.nav is not used: its Galileo F/NAV records carry the I/NAV
data-source value (517), so no F/NAV ephemeris is selectable from it.

Reductions (the output is still a valid input of the original runs):
  observations  GPS and Galileo only, observation types C/L/D/S of L1 C/A
                (1C) and L5 / E5a (5Q); other header records that refer to
                dropped systems or types (phase shifts, GLONASS slots) are
                removed
  navigation    GPS LNAV records within the hour +- 2 h, Galileo F/NAV +- 4 h
                (the ephemeris age limits; PVS can refer to an older IODnav).
                The SPP seed takes the group delay of the first record of a
                satellite, so the first epochs differ from a run on the full
                file by up to 0.7 m (kinematic), 1 mm at the end of the hour.
  SIS messages  PRN 122 L5 frames only (the Kamakura log also has L1 frames and
                the BDSBAS L5 GEOs PRN 130/143/144, which send MT32 too)
The DAS log is kept as is: the alic test converts it with das2sbs.py.

CSSRLIB_DATA must be a checkout of commit CSSRLIB_DATA_COMMIT: later commits
replace data/doy2025-233/233h_sbas.txt (562acb8) with a log whose PRN 122
records differ.

Usage (from the repository root):
    make_southpan_fixture.py CSSRLIB_DATA BRD233.rnx BRD110.rnx OUTDIR [--version N]
"""

import argparse
import datetime
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_dataset_asset  # noqa: E402

KEEP_SYS = ("G", "E")
KEEP_TYPES = {f"{k}{b}" for k in "CLDS" for b in ("1C", "5Q")}
DROP_HEADER = ("SYS / PHASE SHIFT", "GLONASS SLOT / FRQ #", "GLONASS COD/PHS/BIS")
NAV_MARGIN_S = {("G", "LNAV"): 7200, ("E", "FNAV"): 14400}  # MAXDTOE, MAXDTOE_GAL

CSSRLIB_DATA_COMMIT = "fd038eb558b876be57545631335a4d3fce4fb9bf"

NOTICE = """\
{name} (MRTKLIB test dataset, #418), built by
scripts/tests/make_southpan_fixture.py of https://github.com/h-shiono/MRTKLIB

The files are modified excerpts of the sources below: cut to one hour,
observations reduced to GPS / Galileo L1 C/A, L5, E1, E5a, navigation records
reduced to the ephemeris age limits, SIS messages reduced to PRN 122 L5. The
DAS log is unchanged.

ALBY233h.obs, ALIC110f.obs, ALBY233h.sbs, DAS2025110f.txt
  From cssrlib-data (https://github.com/hirokawa/cssrlib-data),
  commit {commit},
  data/doy2025-233 and data/doy2025-110, MIT licence (copyright and
  permission notice below).

ALBY233h.obs, ALIC110f.obs
  Observations of the stations ALBY (50191M001) and ALIC (50137M001), operated
  by Geoscience Australia and part of the IGS network. Commonwealth of
  Australia (Geoscience Australia), Australian National Positioning
  Infrastructure Data Collection, https://pid.geoscience.gov.au/dataset/ga/144503,
  licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/).

ALBY233h.sbs, DAS2025110f.txt
  SouthPAN PRN 122 messages (signal in space logged in Kamakura, and the
  SouthPAN Data Access Service). SouthPAN data of the Commonwealth of
  Australia as represented by Geoscience Australia and the Sovereign in Right
  of New Zealand acting by and through Toitu Te Whenua Land Information New
  Zealand, licensed under CC BY 4.0
  (https://creativecommons.org/licenses/by/4.0/; SBAS-STN-0001 Rev 04,
  "Licence"). Provided as is, without warranty (SBAS-STN-0001).

BRD4233h.rnx, BRD4110f.rnx
  Cut from BRD400DLR_S_20252330000_01D_MN.rnx and
  BRD400DLR_S_20251100000_01D_MN.rnx: Montenbruck, O., & Steigenberger, P.
  (2022). BRD400DLR: DLR's merged multi-GNSS broadcast ephemeris product in
  RINEX 4.00 format [Data set]. DLR/GSOC. https://doi.org/10.57677/BRD400DLR
  IGS data and products, used under the IGS Data and Product Terms of Use
  (attribution to the IGS and its contributing organizations).

cssrlib-data licence:
"""

CASES = [
    # (obs out, sbs out or DAS out, nav out, obs in, corr in, window GPST)
    (
        "ALBY233h.obs",
        "ALBY233h.sbs",
        "BRD4233h.rnx",
        "doy2025-233/alby233h_rnx.obs",
        "doy2025-233/233h_sbas.txt",
        (2025, 8, 21, 7),
    ),
    (
        "ALIC110f.obs",
        "DAS2025110f.txt",
        "BRD4110f.rnx",
        "doy2025-110/ALIC00AUS0110f.obs",
        "doy2025-110/DAS2025110f.txt",
        (2025, 4, 20, 5),
    ),
]


def gps_seconds(y, mo, d, h, mi, s):
    t = datetime.datetime(y, mo, d, h, mi, s) - datetime.datetime(1980, 1, 6)
    return t.days * 86400 + t.seconds


def filter_obs(lines):
    """Observations of KEEP_SYS with KEEP_TYPES; returns the lines."""
    k = next(j for j, l in enumerate(lines) if "END OF HEADER" in l[60:])
    head, types, keep = [], {}, {}
    j = 0
    while j < k:
        l = lines[j]
        label = l[60:].rstrip()
        if label == "SYS / # / OBS TYPES":
            sys_, n = l[0], int(l[3:6])
            t = l[7:60].split()
            while len(t) < n:
                j += 1
                t += lines[j][7:60].split()
            if sys_ in KEEP_SYS:
                idx = [i for i, x in enumerate(t) if x in KEEP_TYPES]
                keep[sys_] = idx
                kept = [t[i] for i in idx]
                for c in range(0, len(kept), 13):
                    body = (f"{sys_}  {len(kept):3d}" if c == 0 else " " * 6) + "".join(
                        f" {x}" for x in kept[c : c + 13]
                    )
                    head.append(f"{body:<60}SYS / # / OBS TYPES\n")
                types[sys_] = kept
        elif label not in DROP_HEADER:
            head.append(l)
        j += 1
    head.append(lines[k])

    out, j = head, k + 1
    while j < len(lines):
        l = lines[j]
        if not l.startswith(">"):
            j += 1
            continue
        n = int(l[32:35])
        sats = []
        for s in lines[j + 1 : j + 1 + n]:
            if s[0] not in keep:
                continue
            s = s.rstrip("\n")
            fields = [s[3 + 16 * i : 3 + 16 * (i + 1)] for i in range((len(s) - 3 + 15) // 16)]
            sel = [(fields[i] if i < len(fields) else "").ljust(16) for i in keep[s[0]]]
            sats.append((s[:3] + "".join(sel)).rstrip() + "\n")
        out.append(f"{l[:32]}{len(sats):3d}{l[35:]}")
        out += sats
        j += 1 + n
    return out


def cut_nav(lines, t0):
    """Header plus the NAV_MARGIN_S records within [t0, t0 + 1 h] +- margin."""
    k = next(j for j, l in enumerate(lines) if "END OF HEADER" in l)
    out, counts, j = lines[: k + 1], {}, k + 1
    while j < len(lines):
        if not lines[j].startswith(">"):
            j += 1
            continue
        e = j + 1
        while e < len(lines) and not lines[e].startswith(">"):
            e += 1
        head = lines[j].split()
        key = (head[2][0], head[3]) if head[1] == "EPH" and len(head) > 3 else None
        if key in NAV_MARGIN_S:
            y, mo, d, h, mi, s = (int(float(x)) for x in lines[j + 1][4:23].split())
            t = gps_seconds(y, mo, d, h, mi, s)
            if t0 - NAV_MARGIN_S[key] <= t <= t0 + 3600 + NAV_MARGIN_S[key]:
                out += lines[j:e]
                counts[key] = counts.get(key, 0) + 1
        j = e
    return out, counts


def getbits(buf, pos, n):
    v = 0
    for k in range(pos, pos + n):
        v = (v << 1) | ((buf[k // 8] >> (7 - k % 8)) & 1)
    return v


def filter_sbs(lines):
    """PRN 122 L5 frames: the type column matches the L5 field (bits 4-9) and
    not the L1 field (bits 8-13), the rule of MRTKLIB's .sbs reader."""
    out = []
    for l in lines:
        v = l.split()
        if len(v) < 6 or v[2] != "122":
            continue
        b, mt = bytes.fromhex(v[5]), int(v[3])
        if getbits(b, 4, 6) == mt and getbits(b, 8, 6) != mt:
            out.append(l)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("cssrlib_data", help="cssrlib-data checkout (its data/ directory is read)")
    ap.add_argument("nav233", help="BRD400DLR_S_20252330000_01D_MN.rnx (uncompressed)")
    ap.add_argument("nav110", help="BRD400DLR_S_20251100000_01D_MN.rnx (uncompressed)")
    ap.add_argument("outdir")
    ap.add_argument("--version", type=int, default=1)
    args = ap.parse_args()

    slices = os.path.join(args.outdir, "slices")
    os.makedirs(slices, exist_ok=True)
    files = []
    for (obs_out, corr_out, nav_out, obs_in, corr_in, ep), nav_in in zip(
        CASES, (args.nav233, args.nav110)
    ):
        data = os.path.join(args.cssrlib_data, "data")
        with open(os.path.join(data, obs_in)) as fp:
            obs = filter_obs(fp.readlines())
        with open(os.path.join(data, corr_in)) as fp:
            corr = fp.readlines()
        if corr_out.endswith(".sbs"):
            corr = filter_sbs(corr)
        with open(nav_in) as fp:
            nav, counts = cut_nav(fp.readlines(), gps_seconds(*ep, 0, 0))
        for name, body in ((obs_out, obs), (corr_out, corr), (nav_out, nav)):
            path = os.path.join(slices, name)
            with open(path, "w") as fp:
                fp.writelines(body)
            files.append(path)
            print(f"{name}: {os.path.getsize(path)} bytes")
        print(f"{nav_out}: records {dict(sorted(counts.items()))}")

    notice = os.path.join(slices, "NOTICE.txt")
    with open(os.path.join(args.cssrlib_data, "LICENSE")) as fp:
        mit = fp.read()
    with open(notice, "w") as fp:
        fp.write(
            NOTICE.format(name=f"southpan-v{args.version}", commit=CSSRLIB_DATA_COMMIT) + "\n" + mit
        )
    files.append(notice)

    name = f"southpan-v{args.version}"
    asset = make_dataset_asset.build(name, args.outdir, files)
    with open(asset, "rb") as fp:
        sha = hashlib.sha256(fp.read()).hexdigest()
    print(f"asset {asset} ({os.path.getsize(asset)} bytes) sha256 {sha}")
    print(f"mrtk_dataset({name} SHA256 {sha})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
