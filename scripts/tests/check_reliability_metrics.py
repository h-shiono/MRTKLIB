#!/usr/bin/env python3
"""Check the benchmark fix-reliability metrics and run-record helpers (#582).

``scripts/benchmark/compare_ppc.reliability_metrics()`` counts correct fixes,
misfix events (each from a misfix to the next correct fix), their spans, the
recovery time and the distance on misfixed epochs, on the expected-solution
grid.  The cases below have known sequences of correct fixes, misfixes, floats,
missing epochs, reference gaps, a standstill and a decimated grid; every value
is checked against a hand-computed one.  The JSON and TOML helpers of
``run_benchmark.py`` are checked too.

Usage:
    check_reliability_metrics.py
"""

import math
import sys
import tempfile
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "benchmark"))
from compare_ppc import reliability_metrics  # noqa: E402
from run_benchmark import _jsonable, _referenced_files, records_stem  # noqa: E402

LAT0, LON0, H0 = 35.0, 137.0, 50.0
STEP = 1e-5  # deg of latitude per unit of the position function (~1.1 m)
FIX, FLOAT = 4, 5


def ref_at(t, f=lambda t: t):
    """Reference record at time ``t``, ``f(t)`` steps north of the origin."""
    return (float(t), LAT0 + STEP * f(t), LON0, H0)


def sol_at(t, q, bad=False, f=lambda t: t):
    """Solution record at time ``t``; a misfix sits 1 m above the reference."""
    return (float(t), LAT0 + STEP * f(t), LON0, H0 + (1.0 if bad else 0.0), q, 10)


def solutions(times, pattern, f=lambda t: t):
    """Solutions from a pattern: C correct fix, M misfix, F float, . missing."""
    out = []
    for t, c in zip(times, pattern):
        if c in "CM":
            out.append(sol_at(t, FIX, bad=c == "M", f=f))
        elif c == "F":
            out.append(sol_at(t, FLOAT, f=f))
    return out


def check(name, got, want, tol=1e-9):
    """Print and return whether ``got`` matches ``want`` (NaN matches NaN)."""
    if isinstance(want, float) and math.isnan(want):
        ok = isinstance(got, float) and math.isnan(got)
    elif isinstance(want, (int, float)) and isinstance(got, (int, float)):
        ok = abs(got - want) <= tol
    else:
        ok = got == want
    print(f"  {'ok ' if ok else 'BAD'} {name}: {got} (expected {want})")
    return ok


def case_events():
    """1 Hz run with floats and missing epochs inside misfix events."""
    #   t  0-4 C | 5-7 M | 8 F | 9 C | 10 . | 11-12 M | 13 . | 14 M |
    #   15-19 C | 20-21 M | 22-29 F
    pattern = "CCCCCMMMFC.MM.MCCCCCMMFFFFFFFF"
    times = range(len(pattern))
    rel = reliability_metrics([ref_at(t) for t in times], solutions(times, pattern))
    events = [
        # 5-7 misfixed, float at 8, correct fix at 9
        {"start": 5.0, "span_s": 3.0, "misfix_s": 3.0, "recovery_s": 2.0, "end": "recovered"},
        # 11-14: the missing epoch 13 does not split it
        {"start": 11.0, "span_s": 4.0, "misfix_s": 3.0, "recovery_s": 1.0, "end": "recovered"},
        # 20-21, then float to the end of the run
        {"start": 20.0, "span_s": 2.0, "misfix_s": 2.0, "recovery_s": None, "end": "end"},
    ]
    return [
        check("n_expected", rel["n_expected"], 30),
        check("dt_grid_s", rel["dt_grid_s"], 1.0),
        check("evaluated_h", rel["evaluated_h"], 30 / 3600.0),
        check("missing_rate", rel["missing_rate"], 100.0 * 2 / 30),
        check("n_fix3d", rel["n_fix3d"], 19),
        check("n_fix3d_correct", rel["n_fix3d_correct"], 11),
        check("correct_fix_rate", rel["correct_fix_rate"], 100.0 * 11 / 30),
        check("misfix3d_rate", rel["misfix3d_rate"], 100.0 * 8 / 19),
        # Unchanged definition: consecutive misfixed epochs only.
        check("misfix3d_longest_s", rel["misfix3d_longest_s"], 3.0),
        check("misfix3d_dist_rate", rel["misfix3d_dist_rate"], 100.0 * 8 / 29, tol=1e-6),
        check("misfix_events", rel["misfix_events"], 3),
        check("misfix_events_per_h", rel["misfix_events_per_h"], 3 / (30 / 3600.0)),
        # Nearest rank over spans [3, 4, 2] and recoveries [2, 1].
        check("misfix_span_p50_s", rel["misfix_span_p50_s"], 3.0),
        check("misfix_span_p95_s", rel["misfix_span_p95_s"], 4.0),
        check("misfix_span_max_s", rel["misfix_span_max_s"], 4.0),
        check("misfix_time_max_s", rel["misfix_time_max_s"], 3.0),
        check("recovery_p50_s", rel["recovery_p50_s"], 1.0),
        check("recovery_max_s", rel["recovery_max_s"], 2.0),
        check("n_recovered", rel["n_recovered"], 2),
        check("n_censored", rel["n_censored"], 0),
        check("n_unrecovered", rel["n_unrecovered"], 1),
        check("misfix_event_list", rel["misfix_event_list"], events),
    ]


def case_gap():
    """A reference gap censors an open event and is not credited with distance."""
    times = (0, 1, 2, 3, 4, 6, 7, 8)  # reference epoch 5 absent
    rel = reliability_metrics([ref_at(t) for t in times], solutions(times, "CCCMMMCC"))
    return [
        check("gap: misfix_events", rel["misfix_events"], 2),
        check("gap: n_censored", rel["n_censored"], 1),
        check("gap: n_recovered", rel["n_recovered"], 1),
        check("gap: recovery_max_s", rel["recovery_max_s"], 1.0),
        check("gap: misfix3d_longest_s", rel["misfix3d_longest_s"], 2.0),
        check("gap: first event end", rel["misfix_event_list"][0]["end"], "gap"),
        # Intervals 0-1 .. 3-4 and 6-7, 7-8 (6 steps); misfix credit 2-3, 3-4.
        # The epoch 6 after the gap gets no distance.
        check("gap: misfix3d_dist_rate", rel["misfix3d_dist_rate"], 100.0 * 2 / 6, tol=1e-6),
    ]


def case_decimated():
    """10 Hz reference, 5 Hz grid, standstill then accelerating."""

    def f(t):
        return 0.0 if t <= 1.0 else 10.0 * (t - 1.0) ** 2

    ref = [ref_at(round(0.1 * k, 1), f) for k in range(20)]
    grid = [round(0.2 * k, 1) for k in range(10)]  # 0.0 .. 1.8
    sol = solutions(grid, "CCCCCCMMCC", f)  # misfixes at 1.2 and 1.4
    rel = reliability_metrics(ref, sol, interval=0.2, phase=0.0)
    # Credit to the later epoch: (f(1.4) - f(1.0)) / (f(1.8) - f(0)) = 1.6 / 6.4.
    return [
        check("5 Hz: n_expected", rel["n_expected"], 10),
        check("5 Hz: dt_grid_s", rel["dt_grid_s"], 0.2, tol=1e-6),
        check("5 Hz: misfix3d_dist_rate", rel["misfix3d_dist_rate"], 25.0, tol=1e-3),
        check("5 Hz: misfix_span_max_s", rel["misfix_span_max_s"], 0.4),
        check("5 Hz: misfix3d_longest_s", rel["misfix3d_longest_s"], 0.4, tol=1e-6),
        check("5 Hz: recovery_max_s", rel["recovery_max_s"], 0.2),
        check("5 Hz: evaluated_h", rel["evaluated_h"], 2.0 / 3600.0, tol=1e-9),
    ]


def case_no_fix_and_static():
    """No fix at all; fixes without misfix; a static run (no distance)."""
    times = range(30)
    ref = [ref_at(t) for t in times]
    rel = reliability_metrics(ref, solutions(times, "F" * 30))
    rel2 = reliability_metrics(ref, solutions(times, "C" * 30))
    static = [ref_at(t, lambda t: 0.0) for t in times]
    rel3 = reliability_metrics(static, solutions(times, "M" * 30, lambda t: 0.0))
    return [
        check("float only: correct_fix_rate", rel["correct_fix_rate"], 0.0),
        check("float only: misfix3d_rate", rel["misfix3d_rate"], math.nan),
        check("float only: misfix_events", rel["misfix_events"], 0),
        check("float only: misfix_span_max_s", rel["misfix_span_max_s"], math.nan),
        check("all correct: misfix3d_longest_s", rel2["misfix3d_longest_s"], 0.0),
        check("all correct: misfix_span_max_s", rel2["misfix_span_max_s"], math.nan),
        check("static: misfix3d_dist_rate", rel3["misfix3d_dist_rate"], math.nan),
        check("static: n_unrecovered", rel3["n_unrecovered"], 1),
    ]


def case_helpers():
    """JSON conversion, files named in a TOML, record names."""
    js = _jsonable(
        {
            "a": np.float64("nan"),
            "n": np.int64(3),
            "p": Path("x/y"),
            "l": (1.0, float("inf")),
        }
    )
    try:
        _jsonable({1, 2})
        raised = False
    except TypeError:
        raised = True
    with tempfile.TemporaryDirectory() as d:
        root = Path(d)
        (root / "ant.atx").write_text("antenna\n")
        conf = root / "c.toml"
        conf.write_text('[files]\nreceiver_atx = "ant.atx"\nmissing = "none.atx"\nname = "rtk"\n')
        refs = _referenced_files(str(conf), root, {})
    stem = records_stem(type("A", (), {"dataset": "ppc", "mode": "rtk", "case": "x"})())
    stem_ho = records_stem(
        type("A", (), {"dataset": "gici", "mode": "rtk", "case": "", "held_out": True})()
    )
    return [
        check("jsonable", js, {"a": None, "n": 3, "p": "x/y", "l": [1.0, None]}),
        check("jsonable rejects unknown types", raised, True),
        check(
            "referenced files",
            [(r["key"], Path(r["path"]).name) for r in refs],
            [("files.receiver_atx", "ant.atx")],
        ),
        check("referenced sha256", len(refs[0]["sha256"]) if refs else 0, 64),
        check("records stem", stem, "ppc_rtk_subset"),
        check("records stem, held out", stem_ho, "gici_rtk_heldout"),
    ]


def main():
    """Run the checks; exit status 0 when all pass."""
    results = []
    for case in (case_events, case_gap, case_decimated, case_no_fix_and_static, case_helpers):
        print(case.__name__)
        results += case()
    if not all(results):
        print("FAIL")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
