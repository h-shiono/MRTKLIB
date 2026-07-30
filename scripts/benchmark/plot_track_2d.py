#!/usr/bin/env python3
"""plot_track_2d.py - Side-by-side 2D tracks of several solutions, coloured by quality.

One column per input solution, each showing the horizontal track in local
East/North metres, every epoch coloured by its solution status.  All columns
share one origin and one set of axis limits, so the tracks can be compared
directly.  No accuracy statistics — see compare_pos_abs.py for those.

Inputs are RTKLIB .pos or NMEA GGA files (format sniffed per file).

Quality colours
---------------
    Single  red        Float   gold        Fix   green
    PPP     blue       DGPS    orange      SBAS / DR  grey

Usage
-----
    plot_track_2d.py [options] rtk.nmea clas.nmea madoca.nmea

Options
-------
    --labels A,B,C      Column labels (default: derived from the file names)
    --no-order          Keep the input order instead of RTK / CLAS / MADOCA
    --title TEXT        Figure title
    -s, --size FLOAT    Marker size (default 2)
    --clip [PCT]        Clip the shared view to the central PCT% of epochs so a
                        far outlier does not squash the tracks (bare = 99)
    -o, --out FILE      Output PNG (default: track_2d.png)
"""

import argparse
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
# The solution readers and the geodesy helpers live with the test comparison
# scripts; reach them regardless of the working directory.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tests"))

import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from _geo import blh2xyz, xyz2enu  # noqa: E402
from compare_pos_abs import load_solution  # noqa: E402

# Solution status (.pos convention, what load_solution() normalises NMEA onto)
_QUALITY = {
    5: ("Single", "#d62728"),
    2: ("Float", "#e8c007"),
    1: ("Fix", "#2ca02c"),
    6: ("PPP", "#1f77b4"),
    4: ("DGPS", "#ff7f0e"),
    3: ("SBAS", "#7f7f7f"),
    7: ("DR", "#c7c7c7"),
}
_LEGEND_ORDER = [5, 4, 3, 2, 1, 6, 7]

# Preferred column order and pretty names for the benchmark file naming
_PRETTY = {"rtk": "RTK", "clas": "CLAS", "madoca": "MADOCA", "single": "Single", "ppp": "PPP"}
_ORDER = ["RTK", "CLAS", "MADOCA"]


def label_of(path):
    """Derive a column label from a benchmark file name.

    ``nagoya_run1_clas.nmea`` -> ``CLAS``.

    Args:
        path: Solution file path.

    Returns:
        Label string.
    """
    stem = Path(path).stem
    tail = stem.rsplit("_", 1)[-1]
    return _PRETTY.get(tail.lower(), tail.upper())


def main():  # noqa: D103
    p = argparse.ArgumentParser(description="Plot 2D tracks side by side, coloured by quality")
    p.add_argument("--labels", help="Comma-separated column labels, in input order")
    p.add_argument(
        "--no-order", action="store_true", help="Keep input order instead of RTK/CLAS/MADOCA"
    )
    p.add_argument("--title", help="Figure title")
    p.add_argument("-s", "--size", type=float, default=2.0, help="Marker size (default 2)")
    p.add_argument(
        "--clip",
        type=float,
        nargs="?",
        const=99.0,
        default=None,
        metavar="PCT",
        help="Clip the view to the central PCT%% of epochs, dropping outliers "
        "from the axis range (bare --clip = 99)",
    )
    p.add_argument(
        "--no-line",
        action="store_true",
        help="Drop the connecting line (it draws long straight jumps across outliers)",
    )
    p.add_argument("-o", "--out", default="track_2d.png", help="Output PNG")
    p.add_argument("files", nargs="+", help="Solution files (.pos or NMEA)")
    args = p.parse_args()

    labels = (
        [s.strip() for s in args.labels.split(",")]
        if args.labels
        else [label_of(f) for f in args.files]
    )
    if len(labels) < len(args.files):
        labels += [label_of(f) for f in args.files[len(labels) :]]

    tracks = []
    for path, label in zip(args.files, labels):
        rows, fmt, _geoid_ok = load_solution(path)
        if not rows:
            print(f"FAIL: no data in {path}", file=sys.stderr)
            return 1
        print(f"{label:8s} {path}  ({fmt.upper()}, {len(rows)} epochs)")
        tracks.append((label, rows))

    if not args.no_order:
        tracks.sort(key=lambda t: (_ORDER.index(t[0]) if t[0] in _ORDER else len(_ORDER), t[0]))

    # One local frame for every column: the mean position of all epochs.
    lats = np.array([r[0] for _lbl, rows in tracks for r in rows])
    lons = np.array([r[1] for _lbl, rows in tracks for r in rows])
    hs = np.array([r[2] for _lbl, rows in tracks for r in rows])
    olat, olon, oh = float(lats.mean()), float(lons.mean()), float(hs.mean())
    origin = blh2xyz(olat, olon, oh)

    def to_en(rows):
        enu = np.array(
            [xyz2enu(blh2xyz(lat, lon, h) - origin, olat, olon) for lat, lon, h, *_ in rows]
        )
        return enu[:, 0], enu[:, 1]

    tracks = [(label, rows, *to_en(rows)) for label, rows in tracks]

    fig, axes = plt.subplots(
        1, len(tracks), figsize=(5.2 * len(tracks), 5.6), sharex=True, sharey=True
    )
    axes = np.atleast_1d(axes)

    seen = set()
    for ax, (label, rows, e, n) in zip(axes, tracks):
        q = np.array([r[3] for r in rows])
        if not args.no_line:
            ax.plot(e, n, color="0.75", linewidth=0.3, alpha=0.6, zorder=1)
        stats = []
        for qv in _LEGEND_ORDER:
            sel = q == qv
            if not sel.any():
                continue
            name, color = _QUALITY[qv]
            seen.add(qv)
            stats.append((qv, int(sel.sum())))
            ax.scatter(e[sel], n[sel], s=args.size, c=color, label=name, zorder=2)

        # Share of each quality within this solution, in legend order.
        for k, (qv, count) in enumerate(stats):
            name, color = _QUALITY[qv]
            ax.text(
                0.02,
                0.98 - 0.05 * k,
                f"{name:<7s}{count / len(rows) * 100:5.1f}%",
                transform=ax.transAxes,
                va="top",
                ha="left",
                fontsize=8.5,
                family="monospace",
                color=color,
                zorder=3,
                bbox={"boxstyle": "square,pad=0.15", "fc": "white", "ec": "none", "alpha": 0.7},
            )
        ax.set_title(f"{label}  (n={len(rows)})")
        ax.set_xlabel("East [m]")
        ax.set_aspect("equal")
        ax.grid(True, alpha=0.3)
    axes[0].set_ylabel("North [m]")

    # Optional percentile clip of the shared view: one far outlier otherwise
    # stretches all three columns until the tracks collapse into a blob.  The
    # points stay plotted (and counted in the percentages), they just fall
    # outside the axes.
    if args.clip:
        lo, hi = (100.0 - args.clip) / 2.0, (100.0 + args.clip) / 2.0
        all_e = np.concatenate([e for _lbl, _rows, e, _n in tracks])
        all_n = np.concatenate([n for _lbl, _rows, _e, n in tracks])
        e0, e1 = np.percentile(all_e, [lo, hi])
        n0, n1 = np.percentile(all_n, [lo, hi])
        pad = 0.05 * max(e1 - e0, n1 - n0, 1.0)
        axes[0].set_xlim(e0 - pad, e1 + pad)
        axes[0].set_ylim(n0 - pad, n1 + pad)
        for label, rows, e, n in tracks:
            outside = int(((e < e0 - pad) | (e > e1 + pad) | (n < n0 - pad) | (n > n1 + pad)).sum())
            if outside:
                print(f"{label:8s} {outside} epoch(s) outside the {args.clip:g}% view")

    handles = [
        plt.Line2D([], [], marker="o", linestyle="", color=_QUALITY[qv][1], label=_QUALITY[qv][0])
        for qv in _LEGEND_ORDER
        if qv in seen
    ]
    leg = fig.legend(handles=handles, loc="lower center", ncol=len(handles), frameon=False)
    if args.title:
        fig.suptitle(args.title)

    # Reserve exactly the legend's height: a fixed fraction collides with the
    # x label whenever the equal-aspect boxes reach the bottom of the figure.
    fig.tight_layout()
    fig.canvas.draw()
    legend_frac = leg.get_window_extent().height / fig.bbox.height
    fig.tight_layout(rect=(0, legend_frac + 0.02, 1, 1))
    fig.savefig(args.out, dpi=150)
    print(f"Plot saved: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
