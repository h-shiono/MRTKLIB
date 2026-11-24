"""gsdc_cases.py - GSDC-2023 benchmark case metadata and discovery.

Unlike the PPC benchmark (six fixed runs with hard-coded GPS time windows), the
GSDC set has 156 train ``trip/device`` combinations.  Cases are therefore
*discovered* by scanning the extracted dataset directory, and a small curated
default subset keeps the routine run manageable and representative.

A GSDC "tripId" is ``<trip>/<device>`` (e.g.
``2021-12-07-19-22-us-ca-lax-d/pixel5``).  Each train case directory holds
``device_gnss.csv`` (raw measurements) and ``ground_truth.csv`` (reference).
"""

from datetime import date, datetime
from pathlib import Path

# ---------------------------------------------------------------------------
# Curated default subset
# ---------------------------------------------------------------------------
# A representative spread across years (2020-2023), devices (Pixel 4/5/7 Pro,
# Xiaomi Mi8, Samsung S20) and environments (suburban highway, San Francisco,
# San Jose, downtown Los Angeles canyon, mixed route).  ``--case all`` runs
# every discovered train case instead.
CURATED: list[str] = [
    "2020-06-25-00-34-us-ca-mtv-sb-101/pixel4",  # suburban highway, Pixel 4
    "2020-07-17-23-13-us-ca-sf-mtv-280/pixel4",  # SF -> MTV, urban + highway
    "2020-12-10-22-52-us-ca-sjc-c/mi8",  # San Jose, Xiaomi Mi8
    "2021-04-02-20-43-us-ca-mtv-f/sm-g988b",  # Mountain View, Samsung S20
    "2021-12-07-19-22-us-ca-lax-d/pixel5",  # downtown LA canyon, Pixel 5
    "2023-09-05-23-07-us-ca-routen/pixel7pro",  # mixed route, Pixel 7 Pro
]


def trip_date(trip: str) -> date | None:
    """Parse the leading ``YYYY-MM-DD`` of a trip name into a date."""
    try:
        return datetime.strptime(trip[:10], "%Y-%m-%d").date()
    except (ValueError, IndexError):
        return None


def discover_cases(dataset_dir: Path, split: str = "train") -> list[dict]:
    """Scan ``<dataset_dir>/<split>`` for usable benchmark cases.

    Args:
        dataset_dir: GSDC dataset root (the directory that contains ``train``).
        split: ``"train"`` (has ground truth) or ``"test"``.

    Returns:
        Sorted list of case dicts, each with keys ``id`` (``trip/device``),
        ``trip``, ``device``, ``gnss_csv`` (Path), ``ground_truth`` (Path or
        None for test) and ``date`` (datetime.date or None).
    """
    root = dataset_dir / split
    cases = []
    if not root.is_dir():
        return cases
    for gnss in sorted(root.glob("*/*/device_gnss.csv")):
        device = gnss.parent.name
        trip = gnss.parent.parent.name
        gt = gnss.parent / "ground_truth.csv"
        cases.append(
            {
                "id": f"{trip}/{device}",
                "trip": trip,
                "device": device,
                "gnss_csv": gnss,
                "ground_truth": gt if gt.is_file() else None,
                "date": trip_date(trip),
            }
        )
    return cases


def select_cases(dataset_dir: Path, which: str = "curated") -> list[dict]:
    """Return the cases to run.

    Args:
        dataset_dir: GSDC dataset root.
        which: ``"curated"`` (intersection with CURATED, preserving CURATED
            order), ``"all"`` (every discovered train case), or a
            comma-separated list of ``trip/device`` ids.

    Returns:
        List of case dicts (see :func:`discover_cases`).  A requested id
        without ``device_gnss.csv`` is returned as ``{"id": ..., "missing":
        True}`` so the runner can count it as not scored instead of dropping
        it silently.
    """
    discovered = {c["id"]: c for c in discover_cases(dataset_dir, "train")}
    if which == "all":
        return list(discovered.values())
    if which == "curated":
        wanted = CURATED
    else:
        wanted = [w.strip() for w in which.split(",") if w.strip()]
    return [discovered.get(i, {"id": i, "missing": True}) for i in wanted]


def safe_name(case_id: str) -> str:
    """Filesystem-safe stem for a case id (``trip/device`` -> ``trip__device``)."""
    return case_id.replace("/", "__")


# ---------------------------------------------------------------------------
# Sanity check (python gsdc_cases.py [dataset_dir])
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import sys

    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("data/gsdc")
    found = discover_cases(root, "train")
    print(f"discovered {len(found)} train cases under {root}")
    curated = [c for c in select_cases(root, "curated") if not c.get("missing")]
    print(f"curated present: {len(curated)}/{len(CURATED)}")
    for c in curated:
        print(f"  {c['id']:50s}  date={c['date']}")
