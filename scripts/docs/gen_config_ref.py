#!/usr/bin/env python3
"""gen_config_ref.py — Generate TOML configuration reference from conf2toml.py MAPPING.

Usage:
    python scripts/docs/gen_config_ref.py > docs/reference/config-options.md

Reads the MAPPING table and ENUM_DEFS from conf2toml.py to produce a
Markdown configuration reference grouped by TOML section.

Note: This reference covers options in conf2toml.py's MAPPING plus a small
set of additional sections (such as [streams]).  It is intended as a
convenient reference, but may not list every key the loader supports.
"""
from __future__ import annotations

import sys
from collections import OrderedDict
from pathlib import Path

# Add scripts/tools to path so we can import conf2toml
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from conf2toml import MAPPING, _KEY_ENUM, _ENUM_STRINGS  # noqa: E402


def get_enum_values(legacy_key: str) -> str:
    """Get enum values string for a legacy key, if available."""
    if legacy_key not in _KEY_ENUM:
        return ""
    enum_name = _KEY_ENUM[legacy_key]
    if enum_name not in _ENUM_STRINGS:
        return ""
    return _ENUM_STRINGS[enum_name]


def type_label(vtype: str) -> str:
    """Convert internal type to display label."""
    labels = {
        "enum": "enum",
        "int": "integer",
        "float": "float",
        "str": "string",
        "bool": "boolean",
        "snr": "array[int]",
        "navsys": "string[]",
        "siglist": "string[]",
    }
    return labels.get(vtype, vtype)


def main() -> int:
    # Group MAPPING entries by TOML section
    sections: OrderedDict[str, list[tuple[str, str, str, str]]] = OrderedDict()
    for legacy_key, section, toml_key, vtype in MAPPING:
        if section not in sections:
            sections[section] = []
        sections[section].append((legacy_key, section, toml_key, vtype))

    # Section display names
    section_names = {
        "positioning": "Positioning",
        "positioning.clas": "Positioning — CLAS",
        "positioning.snr_mask": "Positioning — SNR Mask",
        "positioning.corrections": "Positioning — Corrections",
        "positioning.atmosphere": "Positioning — Atmosphere",
        "ambiguity_resolution": "Ambiguity Resolution",
        "ambiguity_resolution.thresholds": "Ambiguity Resolution — Thresholds",
        "ambiguity_resolution.counters": "Ambiguity Resolution — Counters",
        "ambiguity_resolution.partial_ar": "Ambiguity Resolution — Partial AR",
        "ambiguity_resolution.hold": "Ambiguity Resolution — Hold",
        "rejection": "Rejection Criteria",
        "slip_detection": "Slip Detection",
        "kalman_filter": "Kalman Filter",
        "kalman_filter.measurement_error": "Kalman Filter — Measurement Error",
        "kalman_filter.initial_std": "Kalman Filter — Initial Std. Deviation",
        "kalman_filter.process_noise": "Kalman Filter — Process Noise",
        "adaptive_filter": "Adaptive Filter",
        "signals": "Signal Selection",
        "receiver": "Receiver",
        "antenna.rover": "Antenna — Rover",
        "antenna.base": "Antenna — Base",
        "output": "Output",
        "files": "Files",
        "server": "Server (rtkrcv)",
    }

    lines: list[str] = []
    lines.append("# Configuration Options Reference")
    lines.append("")
    lines.append("TOML configuration options available in MRTKLIB (generated from the internal mapping table).")
    lines.append("Options are grouped by their TOML section.")
    lines.append("")
    lines.append("!!! tip \"Auto-generated\"")
    lines.append("    This page is auto-generated from the internal option mapping table.")
    lines.append("    Run `python scripts/docs/gen_config_ref.py > docs/reference/config-options.md` to regenerate.")
    lines.append("")

    # Generate each section
    for section, entries in sections.items():
        display = section_names.get(section, section)

        lines.append(f"## {display}")
        lines.append("")
        lines.append(f"TOML section: `[{section}]`")
        lines.append("")
        lines.append("| TOML Key | Type | Values | Legacy Key |")
        lines.append("|----------|------|--------|------------|")

        seen_keys: set[str] = set()
        for legacy_key, _, toml_key, vtype in entries:
            # Skip duplicate TOML keys (e.g. aliases like stats-prndcb → ifb)
            if toml_key in seen_keys:
                continue
            seen_keys.add(toml_key)

            tl = type_label(vtype)
            enum_vals = get_enum_values(legacy_key)

            if vtype == "bool":
                values = "`true` / `false`"
            elif enum_vals:
                # Format enum values for display
                values = f"`{enum_vals}`"
            elif vtype == "navsys":
                values = 'e.g. `["GPS", "Galileo", "QZSS"]`'
            elif vtype == "siglist":
                values = 'e.g. `["G1C", "G2W", "E1C"]`'
            elif vtype == "snr":
                values = "9-element array (0-45 dBHz per 5°)"
            else:
                values = "—"

            lines.append(f"| `{toml_key}` | {tl} | {values} | `{legacy_key}` |")

        lines.append("")

    # Stream configuration (not in MAPPING)
    lines.append("## `[streams]`")
    lines.append("")
    lines.append("**Stream Configuration (rtkrcv only)**")
    lines.append("")
    lines.append("Stream configuration uses a hierarchical structure:")
    lines.append("")
    lines.append("```toml")
    lines.append("[streams.input.rover]")
    lines.append('type = "serial"')
    lines.append('path = "ttyACM0:115200"')
    lines.append('format = "ubx"')
    lines.append("")
    lines.append("[streams.input.correction]")
    lines.append('type = "file"')
    lines.append('path = "correction.l6::T::+1"')
    lines.append('format = "sbf"')
    lines.append("")
    lines.append("[streams.output.stream1]")
    lines.append('type = "file"')
    lines.append('path = "output.pos"')
    lines.append("")
    lines.append("[streams.log.stream1]")
    lines.append('type = "file"')
    lines.append('path = "rover.log"')
    lines.append("```")
    lines.append("")
    lines.append("| Key | Description |")
    lines.append("|-----|-------------|")
    lines.append("| `type` | Stream type: `serial`, `file`, `tcpsvr`, `tcpcli`, `ntrip`, `off` |")
    lines.append("| `path` | Stream path (device, file, URL) |")
    lines.append("| `format` | Data format: `rtcm3`, `ubx`, `sbf`, `binex`, `rinex`, etc. |")
    lines.append("| `nmeareq` | Send NMEA request to stream (boolean) |")
    lines.append("| `nmealat` | NMEA request latitude (float) |")
    lines.append("| `nmealon` | NMEA request longitude (float) |")
    lines.append("")

    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
