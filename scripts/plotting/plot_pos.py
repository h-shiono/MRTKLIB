from argparse import ArgumentParser
from pathlib import Path

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.gridspec import GridSpec
from parse_pvt import blh2xyz, parse_nmea, parse_sbf, xyz2enu

QUALITY_NUM = [1, 4, 5]
QUALITY_LABEL = ["Single", "Fix", "Float"]
COLORS_REF = ["r", "green", "orange"]
COLORS_TRG = ["#ff6347", "#90ee90", "#f0e68c"]


class Plotter:
    def __init__(self, reffile: Path, trgfile: Path = None, trglabel: str = "Target"):
        self.reffile = reffile
        self.trgfile = trgfile
        self.trglabel = trglabel

    def plot(self, savefig: bool = False):
        """Plot position data from reference and target files."""
        # Read reference data
        ext_ref = self.reffile.suffix.lower()
        reader = parse_sbf if ext_ref == ".sbf" else parse_nmea
        df_ref = pd.DataFrame(reader(self.reffile))

        # Compute median position as ground truth reference
        llh_ref = df_ref[["lat", "lon", "hgt"]].median().values
        xyz_ref = blh2xyz(
            [
                np.radians(llh_ref[0]),
                np.radians(llh_ref[1]),
                llh_ref[2],
            ]
        )

        enu_ref = np.array(
            [
                xyz2enu(
                    blh2xyz(
                        [
                            np.radians(df_ref["lat"].iloc[i]),
                            np.radians(df_ref["lon"].iloc[i]),
                            df_ref["hgt"].iloc[i],
                        ]
                    ),
                    xyz_ref,
                )
                for i in range(len(df_ref))
            ]
        )
        df_ref["e"] = enu_ref[:, 0]
        df_ref["n"] = enu_ref[:, 1]
        df_ref["u"] = enu_ref[:, 2]

        # Read target data if provided
        if self.trgfile is not None:
            ext_trg = self.trgfile.suffix.lower()
            reader = parse_sbf if ext_trg == ".sbf" else parse_nmea
            df_trg = pd.DataFrame(reader(self.trgfile))

            enu_trg = np.array(
                [
                    xyz2enu(
                        blh2xyz(
                            [
                                np.radians(df_trg["lat"].iloc[i]),
                                np.radians(df_trg["lon"].iloc[i]),
                                df_trg["hgt"].iloc[i],
                            ]
                        ),
                        xyz_ref,
                    )
                    for i in range(len(df_trg))
                ]
            )
            df_trg["e"] = enu_trg[:, 0]
            df_trg["n"] = enu_trg[:, 1]
            df_trg["u"] = enu_trg[:, 2]

        # Plot
        fig = plt.figure(figsize=(9, 4))
        gs = GridSpec(nrows=3, ncols=2, width_ratios=[1.25, 1])
        axes = [
            fig.add_subplot(gs[0, 0]),
            fig.add_subplot(gs[1, 0]),
            fig.add_subplot(gs[2, 0]),
            fig.add_subplot(gs[:, 1:]),
        ]

        axes[0].set_title("ENU timeseries", fontsize=10)
        for i, enu in enumerate(["e", "n", "u"]):
            for q, c1, c2 in zip(QUALITY_NUM, COLORS_REF, COLORS_TRG):
                # ref
                idx1 = np.where(df_ref["mode"] == q)
                z = 10 if q == 5 else 1
                x1 = df_ref["time_gpst"].values[idx1]
                y1 = df_ref[enu].values[idx1]
                axes[i].scatter(x=x1, y=y1, color=c1, marker=".", s=8, zorder=z)
                # trg
                if self.trgfile is not None:
                    idx2 = np.where(df_trg["mode"] == q)
                    x2 = df_trg["time_gpst"].values[idx2]
                    y2 = df_trg[enu].values[idx2]
                    axes[i].scatter(x=x2, y=y2, color=c2, marker=".", s=8, alpha=0.7)

            if i < 2:
                axes[i].tick_params(axis="x", labelbottom=False)
            else:
                axes[i].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
                axes[i].set_xlabel("time")
                axes[i].tick_params(axis="x", labelbottom=True, labelsize=9)

            axes[i].set_ylabel(f"{enu} (m)")
            xy = np.arange(-1, 1.1, 0.5)
            xyl = [f"{x:.2f}" for x in xy]
            axes[i].set_yticks(xy)
            axes[i].set_yticklabels(xyl, fontsize=9)
            axes[i].set_ylim(-1, 1)
            axes[i].grid(True, which="both", linestyle="--", linewidth=0.5)

        # 2D plot
        i += 1
        axes[i].set_title("EN 2D", fontsize=10)
        axes[i].scatter(x=0, y=0, color="b", marker="+", label="Ground Truth", zorder=100)
        for q, ql, c1, c2 in zip(QUALITY_NUM, QUALITY_LABEL, COLORS_REF, COLORS_TRG):
            # ref
            idx1 = np.where(df_ref["mode"] == q)
            x1 = df_ref["e"].values[idx1]
            y1 = df_ref["n"].values[idx1]
            axes[i].scatter(x=x1, y=y1, color=c1, marker=".", s=8, label=ql)
            # trg
            if self.trgfile is not None:
                idx2 = np.where(df_trg["mode"] == q)
                x2 = df_trg["e"].values[idx2]
                y2 = df_trg["n"].values[idx2]
                axes[i].scatter(
                    x=x2,
                    y=y2,
                    color=c2,
                    marker=".",
                    s=8,
                    label=f"{ql} ({self.trglabel})",
                    alpha=0.7,
                )

        xy = np.arange(-1, 1.1, 0.25)
        xyl = [f"{x:.2f}" for x in xy]

        axes[i].set_xlabel("e (m)")
        axes[i].set_xticks(xy)
        axes[i].set_xticklabels(xyl, fontsize=9)
        axes[i].set_xlim(-1, 1)

        axes[i].set_ylabel("n (m)")
        axes[i].set_yticks(xy)
        axes[i].set_yticklabels(xyl, fontsize=9)
        axes[i].set_ylim(-1, 1)

        axes[i].grid(True, which="both", linestyle="--", linewidth=0.5)
        axes[i].set_aspect("equal", adjustable="box")
        axes[i].legend(fontsize=8)

        plt.tight_layout()

        # Save figure
        if savefig is not None:
            if savefig.parent != Path("."):
                savefig.parent.mkdir(parents=True, exist_ok=True)
            path = (
                savefig
                / f"{self.reffile.stem}_{self.trgfile.stem if self.trgfile else 'ref'}_pos.png"
            )
            fig.savefig(path, dpi=300)
            print(f"Figure saved to {path}")
        else:
            plt.show()

        # Close the figure to free memory
        plt.close(fig)


if __name__ == "__main__":
    parser = ArgumentParser(description="Plot position data from NMEA or SBF files.")
    parser.add_argument(
        "reffile",
        help="Reference file (NMEA or SBF) for plotting",
        type=Path,
    )
    parser.add_argument(
        "--trgfile",
        help="Target file (NMEA or SBF) for plotting",
        type=Path,
        default=None,
    )
    parser.add_argument(
        "--trglabel",
        help="Label for the target data in the legend",
        type=str,
        default="Target",
    )
    parser.add_argument(
        "--savefig",
        help="Save the figure to the specified path instead of displaying it",
        type=Path,
        default=None,
    )
    args = parser.parse_args()

    plotter = Plotter(args.reffile, args.trgfile, args.trglabel)
    plotter.plot(args.savefig)
