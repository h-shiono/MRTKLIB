"""Synthetic UBX-RXM-RAWX X20 signal mapping regression (no receiver data).

Signal IDs: u-blox X20 HPG 2.02 interface description,
UBXDOC-304424225-19967 R03, table 4. Unknown IDs must remain rejected.
"""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def rawx_fixture():
    frames = bytearray()
    signals = [(0, 1, 0), (3, 20, 4), (3, 20, 5), (3, 20, 7), (2, 10, 8), (3, 21, 15)]
    for epoch in range(3):
        payload = struct.pack("<dHbBBBBB", 259000.0 + epoch, 2440, 18, len(signals), 1, 1, 0, 0)
        for index, (system, satellite, signal) in enumerate(signals):
            payload += struct.pack(
                "<ddfBBBBHBBBBBB",
                24000000.0 + 100.0 * index + epoch,
                120000000.0 + epoch,
                -1000.0,
                system,
                satellite,
                signal,
                7,
                2000,
                45,
                0,
                0,
                0,
                7,
                0,
            )
        message = bytes([2, 0x15]) + struct.pack("<H", len(payload)) + payload
        a = b = 0
        for value in message:
            a = (a + value) & 255
            b = (b + a) & 255
        frames += b"\xb5\x62" + message + bytes([a, b])
    return frames


def observations(path):
    types = {}
    epochs = []
    system = None
    in_header = True
    for line in path.read_text().splitlines():
        if in_header:
            if "SYS / # / OBS TYPES" in line:
                if line[0] != " ":
                    system = line[0]
                    types[system] = []
                types[system] += line[7:60].split()
            if "END OF HEADER" in line:
                in_header = False
        elif line.startswith(">"):
            epochs.append({})
        elif epochs and len(line) >= 3 and line[0] in types and line[1:3].isdigit():
            epochs[-1][line[:3]] = {
                code: float(line[3 + 16 * i : 17 + 16 * i])
                for i, code in enumerate(types[line[0]])
                if line[3 + 16 * i : 17 + 16 * i].strip()
            }
    return epochs


def verify_fixture(path):
    epochs = observations(path)
    if len(epochs) != 3:
        raise ValueError("Synthetic fixture epoch count")
    for i, epoch in enumerate(epochs):
        expected = {
            "G01": {"C1C": 24000000.0},
            "C20": {"C6I": 24000100.0, "C1P": 24000200.0, "C5P": 24000300.0},
            "E10": {"C6B": 24000400.0},
        }
        for sat, codes in expected.items():
            for code, value in codes.items():
                if epoch.get(sat, {}).get(code) != value + i:
                    raise ValueError("Synthetic mapping/value mismatch: " + sat + "/" + code)
        if "C21" in epoch:
            raise ValueError("Unknown signal identifier incorrectly accepted")
    return {
        "epochs": 3,
        "beidou_codes": ["6I", "1P", "5P"],
        "galileo_code": "6B",
        "unknown_signal_rejected": True,
        "kind": "SYNTHETIC protocol test, not receiver accuracy",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mrtk", type=Path)
    args = parser.parse_args()
    engine = args.mrtk.resolve()
    with tempfile.TemporaryDirectory(prefix="mrtk-ubx-x20-") as directory:
        folder = Path(directory)
        raw = folder / "synthetic.ubx"
        obs = folder / "synthetic.obs"
        raw.write_bytes(rawx_fixture())
        result = subprocess.run(
            [
                str(engine),
                "convert",
                "-r",
                "ubx",
                "-v",
                "3.04",
                "-od",
                "-os",
                "-o",
                str(obs),
                str(raw),
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        verify_fixture(obs)
    print("X20 RAWX mappings and unknown-signal rejection passed")


if __name__ == "__main__":
    main()
