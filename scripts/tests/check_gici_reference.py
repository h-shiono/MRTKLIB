#!/usr/bin/env python3
"""Check the GICI reference construction of prepare_gici.py (#583).

The reference every GICI case is scored against is the ``ground_truth.txt``
IMU position moved to the antenna by the header lever arm, turned by the
attitude of each epoch.  A swapped angle or sign would move every score
without failing any other test, so the rotations are checked against
hand-derived poses, and header parsing, the epoch grid, the heading-only
``--lever-rfu`` path and the antenna shift on a synthetic file.

Usage:
    check_gici_reference.py
"""

import math
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "benchmark"))
from prepare_gici import (  # noqa: E402
    _llh2ecef,
    antenna_position,
    attitude_to_enu,
    lever_enu,
    read_ground_truth,
    read_lever_arm,
    write_reference,
)

TOL = 1e-9
FAILS = []


def check(name, got, want, tol=TOL):
    got, want = list(got), list(want)
    ok = len(got) == len(want) and all(abs(g - w) <= tol for g, w in zip(got, want))
    print(f"{'PASS' if ok else 'FAIL'}: {name}: {[round(g, 6) for g in got]}")
    if not ok:
        FAILS.append(f"{name}: got {got}, want {want}")


def main() -> int:
    x, y, z = 0.3, 0.2, 0.1  # right, forward, up

    # Attitude: heading clockwise from north, pitch nose up, roll right wing down.
    check("level", attitude_to_enu((x, y, z), 0, 0, 0), (x, y, z))
    # Facing east: forward -> east, right -> south.
    check("heading 90", attitude_to_enu((x, y, z), 90, 0, 0), (y, -x, z))
    # Facing west: forward -> west, right -> north.
    check("heading -90", attitude_to_enu((x, y, z), -90, 0, 0), (-y, x, z))
    # Facing south.
    check("heading 180", attitude_to_enu((x, y, z), 180, 0, 0), (-x, -y, z))
    # Nose straight up: forward -> up, up -> backward (south).
    check("pitch 90", attitude_to_enu((x, y, z), 0, 90, 0), (x, -z, y))
    # Upside down about the forward axis: right -> left, up -> down.
    check("roll 180", attitude_to_enu((x, y, z), 0, 0, 180), (-x, y, -z))
    # Right wing down 90: right -> down, up -> right.
    check("roll 90", attitude_to_enu((x, y, z), 0, 0, 90), (z, y, -x))
    # Order: roll first, then pitch, then heading (pitch 90 after roll 90).
    check("roll 90 then pitch 90", attitude_to_enu((x, y, z), 0, 90, 90), (z, x, y))

    # The GICI header lever arm with the reported roll of about 180 deg puts the
    # antenna right, forward and below the IMU; the header's yRot=90 would put
    # it 0.36 m below instead (#583).
    header = (-0.356, 0.278, 0.049)
    check("GICI header, roll 180", attitude_to_enu(header, 0, 0, 180), (0.356, 0.278, -0.049))

    # Heading-only vehicle-frame offset (--lever-rfu).
    row = (2263, 0.0, 31.0, 121.0, 10.0, 90.0, 30.0, 170.0)
    check("lever_rfu heading 90", lever_enu(row, lever_rfu=(x, y, z)), (y, -x, z))
    check(
        "lever_imu uses the full attitude",
        lever_enu(row, lever_imu=(x, y, z)),
        attitude_to_enu((x, y, z), 90.0, 30.0, 170.0),
    )

    # Antenna shift: east / north / up of 1 m are 1 m in ECEF along the local axes.
    lat, lon, h = 31.2, 121.4, 15.0
    p0 = _llh2ecef(lat, lon, h)
    la, lo = math.radians(lat), math.radians(lon)
    axes = {
        "east": (-math.sin(lo), math.cos(lo), 0.0),
        "north": (-math.sin(la) * math.cos(lo), -math.sin(la) * math.sin(lo), math.cos(la)),
        "up": (math.cos(la) * math.cos(lo), math.cos(la) * math.sin(lo), math.sin(la)),
    }
    for i, (name, axis) in enumerate(axes.items()):
        enu = [0.0, 0.0, 0.0]
        enu[i] = 1.0
        p1 = _llh2ecef(*antenna_position(lat, lon, h, enu))
        check(f"antenna shift {name}", [b - a for a, b in zip(p0, p1)], axis, 1e-6)

    with tempfile.TemporaryDirectory() as tmp:
        gt = Path(tmp) / "ground_truth.txt"
        gt.write_text(
            "Project: synthetic\n"
            "IMU to GNSS Antenna Lever Arms: x=-0.356, y=0.278, z=0.049 m (x-right, y-fwd, z-up)\n"
            "Body to IMU Rotations: xRot=0, yRot=90, zRot=0\n"
            "Week GPSTime Status Longitude Latitude H-Ell Heading Pitch Roll\n"
            "2263 100.00 1 121.4000000000 31.2000000000 15.000 0.0 0.0 180.0\n"
            "2263 100.05 1 121.4000000000 31.2000000000 15.000 0.0 0.0 180.0\n"
            "2263 100.10 1 121.4000010000 31.2000020000 15.100 90.0 0.0 180.0\n"
        )
        check("header lever arm", read_lever_arm(gt), header)
        rows = read_ground_truth(gt)
        check("epochs on the 0.1 s grid", [r[1] for r in rows], (100.0, 100.1))
        check(
            "ground-truth fields",
            rows[1],
            (2263, 100.1, 31.200002, 121.400001, 15.1, 90.0, 0.0, 180.0),
        )

        ref = Path(tmp) / "reference.csv"
        write_reference(rows, ref, lever_imu=read_lever_arm(gt))
        lines = ref.read_text().splitlines()
        want = antenna_position(31.2, 121.4, 15.0, (0.356, 0.278, -0.049))
        got = [float(v) for v in lines[1].split(",")[2:5]]
        # written with 10 decimals of a degree and 4 of a metre
        check("reference.csv antenna lat/lon", got[:2], want[:2], 1e-10)
        check("reference.csv antenna height", got[2:], want[2:], 1e-4)
        print(lines[0])
        if not lines[0].startswith("GPS TOW (s),GPS Week,Latitude"):
            FAILS.append("reference.csv header")

        nolever = Path(tmp) / "nolever.txt"
        nolever.write_text("2263 100.00 1 121.4 31.2 15.0 0.0 0.0 180.0\n")
        try:
            read_lever_arm(nolever)
            FAILS.append("missing lever arm not rejected")
        except SystemExit:
            print("PASS: missing lever arm rejected")

    if FAILS:
        print("\n".join(["", "FAILED:"] + FAILS))
        return 1
    print("\nall GICI reference checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
