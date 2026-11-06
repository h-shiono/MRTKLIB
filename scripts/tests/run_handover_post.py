#!/usr/bin/env python3
"""Post-process one QZSS L6 handover case (#372).

Pipeline per case of the handover dataset:

1. ``mrtk convert`` the observation SBF slice to RINEX obs/nav;
2. ``mrtk l6extract`` the L6 slice (D9C UBX, or mosaic-CLAS SBF) into
   ``{prefix}.{PRN}.l6d`` files (#362);
3. rebuild each receiver channel by concatenating its PRN files in time
   order, as recorded in ``handover_manifest.json`` (``l6_channels``): a
   channel tracks one PRN at a time, so "all of the old PRN, then all of the
   new one" is that channel's frame sequence (``mrtk post`` takes one file per
   CLAS channel; reading several PRN files per channel natively is #373);
4. ``mrtk post`` with the case's config, channel files in channel order.

Usage:
    run_handover_post.py MRTK DATASET_DIR CASE CONF WORK_DIR OUT_NMEA
"""

import json
import os
import shutil
import subprocess
import sys


def run(cmd, cwd=None):
    res = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if res.returncode != 0:
        raise SystemExit(f"failed ({res.returncode}): {' '.join(cmd)}\n{res.stdout}{res.stderr}")
    return res


def main():
    if len(sys.argv) != 7:
        print(__doc__, file=sys.stderr)
        return 2
    mrtk, dset, case_name, conf, work, out = sys.argv[1:]
    with open(os.path.join(dset, "handover_manifest.json")) as fp:
        case = next(c for c in json.load(fp)["cases"] if c["case"] == case_name)

    files = case["files"]
    obs_src = next(n for n, f in files.items() if n.endswith(".sbf"))
    l6_src = next(n for n, f in files.items() if f["l6"])

    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    obs, nav = os.path.join(work, "obs.obs"), os.path.join(work, "nav.nav")
    run([mrtk, "convert", "-r", "sbf", "-o", obs, "-n", nav, os.path.join(dset, obs_src)])
    prefix = os.path.join(work, "l6")
    run([mrtk, "l6extract", "-no-has", "-o", prefix, os.path.join(dset, l6_src)])

    seen = set()
    channels = []
    for ch, runs in sorted(case["l6_channels"].items(), key=lambda x: int(x[0])):
        prns = [r["prn"] for r in runs]
        if seen & set(prns):
            raise SystemExit(f"{case_name}: PRN on two channels, cannot split by PRN file")
        seen |= set(prns)
        path = os.path.join(work, f"ch{ch}.l6d")
        with open(path, "wb") as dst:
            for r in runs:
                with open(f"{prefix}.{r['prn']}.l6d", "rb") as src:
                    data = src.read()
                if len(data) != 250 * r["frames"]:
                    raise SystemExit(
                        f"{case_name}: {prefix}.{r['prn']}.l6d has {len(data) // 250} frames, "
                        f"manifest {r['frames']}"
                    )
                dst.write(data)
        channels.append(path)
        print(f"ch{ch}: " + " -> ".join(f"{p} ({r['frames']})" for p, r in zip(prns, runs)))

    run([mrtk, "post", "-k", conf, "-o", out, obs, nav, *channels])
    print(f"{case_name}: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
