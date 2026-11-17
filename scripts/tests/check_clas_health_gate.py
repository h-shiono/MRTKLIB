#!/usr/bin/env python3
"""End-to-end check of the CLAS PPP-RTK satellite / signal health gate (#285).

The bundled CLAS data carries no unhealthy satellite, so this injects health
flags into a copy of the RINEX navigation file and runs ``mrtk post`` on the
same window six times:

1. base: navigation file as shipped;
2. QZS 0x01 on every QZS ephemeris (L1C/B unhealthy, as QZS-2/3/4 broadcast):
   outside the 0x2C signal mask and cleared by the SPP satexclude(), so the
   solution must be byte-identical to the base run;
3. QZS 0x10 (L1C/A unhealthy, as QZS-1R/6 broadcast): also outside the mask
   and, since #512, cleared by the SPP satexclude() as well, so the solution
   must be byte-identical to the base run (before #512 the SPP seed dropped
   QZS and moved the PPP-RTK result);
4. GPS unhealthy: the GPS satellite used in the most base epochs gets
   svh = 1 and must never be used (before #285 it stayed in the solution);
5. the same unhealthy GPS satellite force-included with excluded_sats
   "+Gnn" (exsats 2): bypasses the health gate as in upstream claslib
   satsigexclude(), so it must be used again;
6. QZS L2C unhealthy: the QZS satellite used in the most base epochs gets
   svh = 0x08, which must drop its L2 signal and keep L1.

The satellite usage comes from the solution status ($SAT, -y 2).

VRS path: ``mrtk ssr2obs`` with that QZS satellite at svh = 0x08 must emit
no L2 code / phase for it and keep L1 (the VRS observations are built from
the corrections, not from the receiver measurements the gate clears).

Usage:
    check_clas_health_gate.py MRTK CONF SSR2OBS_CONF OBS NAV L6 WORK_DIR
"""

import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

TS = ["-ts", "2019/08/27", "16:00:00", "-te", "2019/08/27", "16:09:59", "-ti", "1"]


def patch_nav(src: Path, dst: Path, health: dict) -> int:
    """Copy a RINEX 3 navigation file, setting the SV health of GPS / QZS records.

    health maps a satellite id ("G05", "J02") to a value, or a system letter
    ("J") to a list of values used in turn. Returns the number of records patched.
    """
    lines = src.read_text(encoding="ascii").splitlines(keepends=True)
    body = next(i for i, ln in enumerate(lines) if "END OF HEADER" in ln) + 1
    count = Counter()
    patched = 0
    i = body
    while i < len(lines):
        ln = lines[i]
        sat = ln[:3]
        nrec = {"G": 8, "J": 8, "E": 8, "C": 8, "I": 8, "R": 4, "S": 4}.get(ln[:1], 1)
        val = health.get(sat)
        if val is None and isinstance(health.get(sat[:1]), list):
            vals = health[sat[:1]]
            val = vals[count[sat[:1]] % len(vals)]
            count[sat[:1]] += 1
        if val is not None and ln[:1] in "GJ":
            # broadcast orbit 6: URA, SV health, TGD, IODC (4X,4D19.12)
            k = i + 6
            orb = lines[k]
            lines[k] = orb[:23] + f"{float(val):19.12E}" + orb[42:]
            patched += 1
        i += nrec
    dst.write_text("".join(lines), encoding="ascii")
    return patched


def run_post(mrtk, conf, obs, nav, l6, out: Path, overlay: Path = None) -> None:
    """Run mrtk post with solution status output (overlay: second -k config)."""
    extra = ["-k", str(overlay)] if overlay else []
    cmd = [mrtk, "post", "-k", conf, *extra, *TS, "-y", "2", "-o", str(out), obs, str(nav), l6]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        raise SystemExit(f"failed ({res.returncode}): {' '.join(cmd)}\n{res.stdout}{res.stderr}")


def run_ssr2obs(mrtk, conf, nav, l6, out: Path) -> None:
    """Run mrtk ssr2obs (VRS observations from the CLAS corrections)."""
    cmd = [mrtk, "ssr2obs", "-k", conf, *TS, "-r", "-o", str(out), str(nav), l6]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        raise SystemExit(f"failed ({res.returncode}): {' '.join(cmd)}\n{res.stdout}{res.stderr}")


def rinex_signals(path: Path, sat: str) -> Counter:
    """Count the RINEX 3 epochs with a non-blank value per obs type for one satellite."""
    types = {}
    count = Counter()
    lines = path.read_text(encoding="ascii", errors="replace").splitlines()
    body = next(i for i, ln in enumerate(lines) if "END OF HEADER" in ln) + 1
    for ln in lines[:body]:
        if ln[60:].startswith("SYS / # / OBS TYPES") and ln[0] != " ":
            types[ln[0]] = ln[7:58].split()
    for ln in lines[body:]:
        if ln[:3] != sat:
            continue
        for k, t in enumerate(types.get(sat[0], [])):
            if ln[3 + 16 * k : 3 + 16 * k + 14].strip():
                count[t] += 1
    return count


def sat_usage(stat: Path) -> Counter:
    """Count epochs with vsat = 1 per (satellite id, frequency slot) in a .stat file."""
    use = Counter()
    for ln in stat.read_text(encoding="ascii", errors="replace").splitlines():
        f = ln.split(",")
        if f[0] == "$SAT" and len(f) > 9 and f[9] == "1":
            use[(f[3], int(f[4]))] += 1
    return use


def most_used(use: Counter, sysid: str) -> str:
    """Return the satellite of a system used in the most epochs on slot 1."""
    cands = [(n, s) for (s, f), n in use.items() if f == 1 and s.startswith(sysid)]
    if not cands:
        raise SystemExit(f"FAIL: no {sysid} satellite used in the base run")
    return max(cands)[1]


def main() -> int:
    """Run the four cases and report the result."""
    if len(sys.argv) != 8:
        print(__doc__, file=sys.stderr)
        return 2
    mrtk, conf, vconf, obs, nav, l6, work = sys.argv[1:]
    work = Path(work)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    ok = True

    run_post(mrtk, conf, obs, nav, l6, work / "base.nmea")
    base = sat_usage(work / "base.nmea.stat")
    gsat, jsat = most_used(base, "G"), most_used(base, "J")

    nav_01 = work / "qzs_01.nav"
    n = patch_nav(Path(nav), nav_01, {"J": [0x01]})
    run_post(mrtk, conf, obs, nav_01, l6, work / "qzs_01.nmea")
    same = (work / "qzs_01.nmea").read_bytes() == (work / "base.nmea").read_bytes()
    ok &= same and n > 0
    print(
        f"{'PASS' if same and n else 'FAIL'}: QZS svh 0x01 on {n} ephemerides -> solution "
        f"{'byte-identical' if same else 'CHANGED'}"
    )

    nav_10 = work / "qzs_10.nav"
    n = patch_nav(Path(nav), nav_10, {"J": [0x10]})
    run_post(mrtk, conf, obs, nav_10, l6, work / "qzs_10.nmea")
    same = (work / "qzs_10.nmea").read_bytes() == (work / "base.nmea").read_bytes()
    ok &= same and n > 0
    print(
        f"{'PASS' if same and n else 'FAIL'}: QZS svh 0x10 on {n} ephemerides -> solution "
        f"{'byte-identical' if same else 'CHANGED'}"
    )

    nav_gps = work / "gps_unhealthy.nav"
    n = patch_nav(Path(nav), nav_gps, {gsat: 0x01})
    run_post(mrtk, conf, obs, nav_gps, l6, work / "gps_unhealthy.nmea")
    use = sat_usage(work / "gps_unhealthy.nmea.stat")
    used = sum(v for (s, _), v in use.items() if s == gsat)
    good = n > 0 and used == 0 and base[(gsat, 1)] > 0
    ok &= good
    print(
        f"{'PASS' if good else 'FAIL'}: {gsat} svh 0x01 ({n} ephemerides) -> used "
        f"{base[(gsat, 1)]} epochs in base, {used} after"
    )

    incl = work / "force_include.toml"
    incl.write_text(f'[positioning]\nexcluded_sats = ["+{gsat}"]\n', encoding="ascii")
    run_post(mrtk, conf, obs, nav_gps, l6, work / "gps_forced.nmea", incl)
    use = sat_usage(work / "gps_forced.nmea.stat")
    good = use[(gsat, 1)] >= 0.98 * base[(gsat, 1)]
    ok &= good
    print(
        f"{'PASS' if good else 'FAIL'}: {gsat} svh 0x01 + excluded_sats +{gsat} -> used "
        f"{use[(gsat, 1)]} epochs (base {base[(gsat, 1)]})"
    )

    nav_qzs = work / "qzs_l2.nav"
    n = patch_nav(Path(nav), nav_qzs, {jsat: 0x08})
    run_post(mrtk, conf, obs, nav_qzs, l6, work / "qzs_l2.nmea")
    use = sat_usage(work / "qzs_l2.nmea.stat")
    good = n > 0 and base[(jsat, 2)] > 0 and use[(jsat, 2)] == 0 and use[(jsat, 1)] > 0
    ok &= good
    print(
        f"{'PASS' if good else 'FAIL'}: {jsat} svh 0x08 ({n} ephemerides) -> L1/L2 used "
        f"{base[(jsat, 1)]}/{base[(jsat, 2)]} epochs in base, {use[(jsat, 1)]}/{use[(jsat, 2)]} after"
    )

    for name, vnav in (("vrs_base", Path(nav)), ("vrs_qzs_l2", nav_qzs)):
        run_ssr2obs(mrtk, vconf, vnav, l6, work / f"{name}.obs")
    vb = rinex_signals(work / "vrs_base.obs", jsat)
    vm = rinex_signals(work / "vrs_qzs_l2.obs", jsat)
    l2 = [t for t in vb if t[1] == "2"]
    good = bool(l2) and all(vb[t] > 0 for t in l2) and all(vm.get(t, 0) == 0 for t in l2)
    good &= all(vm.get(t, 0) == vb[t] for t in vb if t[1] == "1")
    ok &= good
    print(
        f"{'PASS' if good else 'FAIL'}: VRS {jsat} svh 0x08 -> epochs per signal "
        + ", ".join(f"{t} {vb[t]}/{vm.get(t, 0)}" for t in sorted(vb))
    )

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
