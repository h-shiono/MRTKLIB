#!/usr/bin/env python3
"""Check the benchmark fix-reliability metrics on a synthetic grid (#582).

``scripts/benchmark/compare_ppc.reliability_metrics()`` counts correct fixes,
misfix events, their durations and the recovery time to the next correct fix
on the expected-solution grid.  This test builds a 1 Hz reference with a known
sequence of correct fixes, misfixes, floats and missing epochs and checks every
count against hand-computed values.

Usage:
    check_reliability_metrics.py
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "benchmark"))
from compare_ppc import reliability_metrics  # noqa: E402

LAT0, LON0, H0 = 35.0, 137.0, 50.0
STEP = 1e-5  # deg of latitude per epoch (~1.1 m)
FIX, FLOAT = 4, 5


def ref_at(t):
    """Reference record at time ``t``, moving north."""
    return (float(t), LAT0 + STEP * t, LON0, H0)


def sol_at(t, q, bad=False):
    """Solution record at time ``t``; a misfix sits 1 m above the reference."""
    return (float(t), LAT0 + STEP * t, LON0, H0 + (1.0 if bad else 0.0), q, 10)


def check(name, got, want, tol=1e-9):
    """Print and return whether ``got`` matches ``want`` (NaN matches NaN)."""
    ok = (
        math.isnan(got) and math.isnan(want)
        if isinstance(want, float) and math.isnan(want)
        else abs(got - want) <= tol
    )
    print(f"  {'ok ' if ok else 'BAD'} {name}: {got} (expected {want})")
    return ok


def main():
    """Run the checks; exit status 0 when all pass."""
    # 30 epochs at 1 Hz. C = correct fix, M = misfix, F = float, . = missing.
    #   t  0-4 C | 5-7 M | 8 F | 9 C | 10 . | 11-12 M | 13 . | 14 M |
    #   15-19 C | 20-21 M | 22-29 F
    pattern = "CCCCCMMMFC.MM.MCCCCCMMFFFFFFFF"
    assert len(pattern) == 30
    ref = [ref_at(t) for t in range(30)]
    sol = []
    for t, c in enumerate(pattern):
        if c == "C":
            sol.append(sol_at(t, FIX))
        elif c == "M":
            sol.append(sol_at(t, FIX, bad=True))
        elif c == "F":
            sol.append(sol_at(t, FLOAT))
    rel = reliability_metrics(ref, sol)

    results = [
        check("n_expected", rel["n_expected"], 30),
        check("dt_grid_s", rel["dt_grid_s"], 1.0),
        check("missing_rate", rel["missing_rate"], 100.0 * 2 / 30),
        check("n_fix3d", rel["n_fix3d"], 19),
        check("n_fix3d_correct", rel["n_fix3d_correct"], 11),
        check("correct_fix_rate", rel["correct_fix_rate"], 100.0 * 11 / 30),
        check("misfix3d_rate", rel["misfix3d_rate"], 100.0 * 8 / 19),
        check("misfix3d_longest_s", rel["misfix3d_longest_s"], 3.0),
        # Uniform spacing: 8 misfixed epochs of 29 credited intervals.
        check("misfix3d_dist_rate", rel["misfix3d_dist_rate"], 100.0 * 8 / 29, tol=1e-6),
        # Events 5-7, 11-12, 14 (split by the missing epoch 13), 20-21.
        check("misfix_events", rel["misfix_events"], 4),
        check("misfix_events_per_h", rel["misfix_events_per_h"], 4 / (30 / 3600.0)),
        check("misfix_dur_p50_s", rel["misfix_dur_p50_s"], 2.0),
        check("misfix_dur_p95_s", rel["misfix_dur_p95_s"], 2.85),
        check("misfix_dur_max_s", rel["misfix_dur_max_s"], 3.0),
        # Recovery: 7 -> 9 (2 s), 12 -> 15 (3 s), 14 -> 15 (1 s); 20-21 never.
        check("recovery_p50_s", rel["recovery_p50_s"], 2.0),
        check("recovery_max_s", rel["recovery_max_s"], 3.0),
        check("n_unrecovered", rel["n_unrecovered"], 1),
    ]
    events = rel["misfix_event_list"]
    want_events = [(5.0, 3.0, 2.0), (11.0, 2.0, 3.0), (14.0, 1.0, 1.0), (20.0, 2.0, None)]
    ok = events == want_events
    print(f"  {'ok ' if ok else 'BAD'} misfix_event_list: {events}")
    results.append(ok)

    # A gap in the reference ends an event: misfixes at 3, 4 and 6 with the
    # reference epoch 5 absent are two events.
    ref2 = [ref_at(t) for t in (0, 1, 2, 3, 4, 6, 7, 8)]
    sol2 = [sol_at(t, FIX, bad=t in (3, 4, 6)) for t in (0, 1, 2, 3, 4, 6, 7, 8)]
    rel2 = reliability_metrics(ref2, sol2)
    results += [
        check("gap: misfix_events", rel2["misfix_events"], 2),
        check("gap: misfix3d_longest_s", rel2["misfix3d_longest_s"], 2.0),
        check("gap: recovery_max_s", rel2["recovery_max_s"], 3.0),
    ]

    # No fix at all: correct-fix rate 0 %, misfix rates undefined, no events.
    rel3 = reliability_metrics(ref, [sol_at(t, FLOAT) for t in range(30)])
    results += [
        check("float only: correct_fix_rate", rel3["correct_fix_rate"], 0.0),
        check("float only: misfix3d_rate", rel3["misfix3d_rate"], math.nan),
        check("float only: misfix_events", rel3["misfix_events"], 0),
        check("float only: misfix_dur_max_s", rel3["misfix_dur_max_s"], math.nan),
    ]

    # Fixes but no misfix: longest is 0 s, as before #582.
    rel4 = reliability_metrics(ref, [sol_at(t, FIX) for t in range(30)])
    results.append(check("all correct: misfix3d_longest_s", rel4["misfix3d_longest_s"], 0.0))

    if not all(results):
        print("FAIL")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
