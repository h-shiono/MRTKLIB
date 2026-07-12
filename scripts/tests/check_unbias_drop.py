#!/usr/bin/env python3
"""Assert that the IGS PPP unbias gate rejected observations as expected."""

import argparse
import sys
from pathlib import Path


def main() -> int:
    """Check that unbias rejection emitted no solution and both trace messages."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("solution", type=Path, help="RTKLIB .pos output")
    parser.add_argument("trace", type=Path, help="MRTKLIB trace output")
    args = parser.parse_args()

    solution = args.solution.read_text(encoding="utf-8", errors="replace")
    if any(line.strip() and not line.startswith("%") for line in solution.splitlines()):
        print(
            f"FAIL: {args.solution} contains a solution although unbias must reject "
            "all pseudoranges"
        )
        return 1

    trace = args.trace.read_text(encoding="utf-8", errors="replace")
    required = ("satellite code bias does not exist", "receiver code bias does not exist")
    missing = [message for message in required if message not in trace]
    if missing:
        print(f"FAIL: missing unbias rejection trace(s): {', '.join(missing)}")
        return 1

    print("PASS: unbias rejected satellite- and receiver-bias misses; no solution was emitted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
