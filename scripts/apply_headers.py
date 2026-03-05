#!/usr/bin/env python3
"""Apply unified license headers to all mrtk_* files.

Strategy:
  - Doxygen blocks (/** ... */): INSERT license header ABOVE, preserve Doxygen.
  - Traditional blocks (/* ... */): REPLACE with license header.

Files with custom content (references/notes/history) are listed in
SKIP_FILES and must be updated manually.
"""
import os

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HEADER_TEMPLATE = """\
/*------------------------------------------------------------------------------
 * {filename} : {description}
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Cabinet Office, Japan
 * Copyright (C) 2024-2025 Lighthouse Technology & Consulting Co. Ltd.
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2015- Mitsubishi Electric Corp.
 * Copyright (C) 2014 Geospatial Information Authority of Japan
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
"""

# Files with extended headers (references/notes/history/options sections).
# These have custom content that must NOT be replaced by the script;
# update them manually to add ME/GSI copyright lines.
SKIP_FILES = {
    # clas/ — all have References and/or Notes sections
    "mrtk_clas.c",
    "mrtk_clas_grid.c",
    "mrtk_clas_osr.c",
    "mrtk_clas_isb.c",
    # madoca/ — has references/history section
    "mrtk_madoca_iono.c",
    # pos/ — have references/notes/history/options sections
    "mrtk_ppp.c",
    "mrtk_ppp_ar.c",
    "mrtk_ppp_iono.c",
    "mrtk_ppp_rtk.c",
    "mrtk_vrs.c",
}

# Map of filename -> one-line description
DESCRIPTIONS = {
    # --- src/core/ ---
    "mrtk_bits.c":             "bit manipulation, CRC, and word decode functions",
    "mrtk_context.c":          "MRTKLIB legacy context management",
    "mrtk_coords.c":           "coordinate transformation functions",
    "mrtk_core.c":             "MRTKLIB core context management and logging",
    "mrtk_mat.c":              "matrix and vector functions",
    "mrtk_sys.c":              "system utility functions",
    "mrtk_time.c":             "time and date functions",
    "mrtk_trace.c":            "debug trace and logging functions",
    # --- src/data/ ---
    "mrtk_bias_sinex.c":       "BIAS-SINEX file reader and bias functions",
    "mrtk_eph.c":              "satellite ephemeris and clock functions",
    "mrtk_fcb.c":              "fractional cycle bias (FCB) functions",
    "mrtk_ionex.c":            "IONEX TEC grid functions",
    "mrtk_nav.c":              "navigation data functions",
    "mrtk_obs.c":              "observation data functions",
    "mrtk_peph.c":             "precise ephemeris and earth rotation parameter functions",
    "mrtk_rcvraw.c":           "receiver raw data functions",
    "mrtk_rinex.c":            "RINEX file I/O functions",
    "mrtk_sbas.c":             "SBAS message decoding and correction functions",
    # --- src/data/rcv/ ---
    "mrtk_rcv_binex.c":        "BINEX receiver raw data decoder",
    "mrtk_rcv_crescent.c":     "Hemisphere (Crescent) receiver raw data decoder",
    "mrtk_rcv_javad.c":        "Javad receiver raw data decoder",
    "mrtk_rcv_novatel.c":      "NovAtel receiver raw data decoder",
    "mrtk_rcv_nvs.c":          "NVS receiver raw data decoder",
    "mrtk_rcv_rt17.c":         "Trimble RT17 receiver raw data decoder",
    "mrtk_rcv_septentrio.c":   "Septentrio receiver raw data decoder",
    "mrtk_rcv_skytraq.c":      "SkyTraq receiver raw data decoder",
    "mrtk_rcv_ss2.c":          "NovAtel Superstar II receiver raw data decoder",
    "mrtk_rcv_ublox.c":        "u-blox receiver raw data decoder",
    # --- src/models/ ---
    "mrtk_antenna.c":          "antenna model and PCV functions",
    "mrtk_astro.c":            "astronomical functions for sun/moon position",
    "mrtk_atmos.c":            "atmosphere model functions",
    "mrtk_geoid.c":            "geoid model functions",
    "mrtk_station.c":          "station position and BLQ functions",
    "mrtk_tides.c":            "tide displacement correction functions",
    # --- src/rtcm/ ---
    "mrtk_rtcm.c":             "RTCM common functions",
    "mrtk_rtcm2.c":            "RTCM version 2 message functions",
    "mrtk_rtcm3.c":            "RTCM version 3 message decoder functions",
    "mrtk_rtcm3e.c":           "RTCM version 3 message encoder functions",
    "mrtk_rtcm3_local_corr.c": "RTCM3 local correction message encoder/decoder",
    # --- src/stream/ ---
    "mrtk_rtksvr.c":           "real-time RTK server functions",
    "mrtk_stream.c":           "stream I/O functions",
    # --- src/madoca/ ---
    "mrtk_madoca.c":           "MADOCA-PPP processing functions",
    "mrtk_madoca_iono.c":      "QZSS L6D MADOCA-PPP wide area ionospheric correction",
    "mrtk_madoca_local_comb.c":"MADOCA local correction data combination",
    "mrtk_madoca_local_corr.c":"MADOCA local correction common functions",
    # --- src/pos/ (simple headers only — custom-content files are in SKIP_FILES) ---
    "mrtk_lambda.c":           "integer ambiguity resolution (LAMBDA/MLAMBDA)",
    "mrtk_opt.c":              "processing and solution option defaults",
    "mrtk_options.c":          "option string processing functions",
    "mrtk_postpos.c":          "post-processing positioning functions",
    "mrtk_rtkpos.c":           "RTK positioning functions",
    "mrtk_sol.c":              "solution I/O functions",
    "mrtk_spp.c":              "standard single-point positioning",
    # --- include/mrtklib/ .h files ---
    "mrtklib.h":               "MRTKLIB public API for context management and logging",
    "mrtk_foundation.h":       "common types, standard includes, and system constants",
    "mrtk_const.h":            "physical, navigation, and system constants",
    "mrtk_context.h":          "MRTKLIB legacy context type definitions",
    "mrtk_time.h":             "time and date type definitions and functions",
    "mrtk_mat.h":              "matrix and vector type definitions and functions",
    "mrtk_coords.h":           "coordinate transformation functions",
    "mrtk_atmos.h":            "atmosphere model functions",
    "mrtk_eph.h":              "satellite ephemeris type definitions and functions",
    "mrtk_peph.h":             "precise ephemeris and ERP type definitions and functions",
    "mrtk_bits.h":             "bit manipulation and CRC functions",
    "mrtk_sys.h":              "system utility functions",
    "mrtk_astro.h":            "astronomical functions",
    "mrtk_antenna.h":          "antenna model and PCV functions",
    "mrtk_station.h":          "station position and BLQ functions",
    "mrtk_obs.h":              "observation data type definitions and functions",
    "mrtk_nav.h":              "navigation data type definitions and functions",
    "mrtk_rinex.h":            "RINEX file I/O type definitions and functions",
    "mrtk_tides.h":            "tide displacement correction functions",
    "mrtk_geoid.h":            "geoid model functions",
    "mrtk_sbas.h":             "SBAS message decoding and correction functions",
    "mrtk_rtcm.h":             "RTCM type definitions and functions",
    "mrtk_ionex.h":            "IONEX TEC grid functions",
    "mrtk_opt.h":              "processing and solution option type definitions",
    "mrtk_sol.h":              "solution type definitions and I/O functions",
    "mrtk_bias_sinex.h":       "BIAS-SINEX type definitions and functions",
    "mrtk_fcb.h":              "fractional cycle bias (FCB) functions",
    "mrtk_spp.h":              "standard single-point positioning functions",
    "mrtk_madoca_local_corr.h":"MADOCA local correction type definitions and functions",
    "mrtk_madoca_local_comb.h":"MADOCA local correction data combination functions",
    "mrtk_madoca.h":           "MADOCA-PPP processing functions",
    "mrtk_lambda.h":           "integer ambiguity resolution (LAMBDA) functions",
    "mrtk_rtkpos.h":           "RTK positioning type definitions and functions",
    "mrtk_ppp_ar.h":           "PPP ambiguity resolution functions",
    "mrtk_ppp.h":              "precise point positioning functions",
    "mrtk_postpos.h":          "post-processing positioning functions",
    "mrtk_options.h":          "option string processing functions",
    "mrtk_rcvraw.h":           "receiver raw data type definitions and functions",
    "mrtk_stream.h":           "stream I/O type definitions and functions",
    "mrtk_rtksvr.h":           "real-time RTK server type definitions and functions",
    "mrtk_trace.h":            "debug trace and logging functions",
    "mrtk_clas.h":             "CLAS (QZSS L6D) type definitions and functions",
    "mrtk_vrs.h":              "VRS-RTK positioning type definitions and functions",
    "mrtk_ppp_rtk.h":          "PPP-RTK positioning type definitions and functions",
    # --- special files ---
    "rtklib.h":                "RTKLIB compatibility wrapper (includes all MRTKLIB headers)",
}


def extract_first_comment(content):
    """Extract the first comment block and return (comment, rest, is_doxygen).

    Returns:
        comment:    The full text of the first comment block (including delimiters).
        rest:       Everything after the comment block (preserving leading blank lines).
        is_doxygen: True if the comment starts with /** (Doxygen block).
    """
    lines = content.split('\n')
    i = 0

    # Skip leading blank lines
    while i < len(lines) and lines[i].strip() == '':
        i += 1

    if i >= len(lines) or not lines[i].strip().startswith('/*'):
        # No comment block at start — return everything as rest
        return '', content, False

    is_doxygen = lines[i].strip().startswith('/**')
    start = i

    # Find the closing */
    while i < len(lines):
        if '*/' in lines[i]:
            i += 1
            break
        i += 1

    comment = '\n'.join(lines[start:i])
    rest = '\n'.join(lines[i:])

    return comment, rest, is_doxygen


def process_file(filepath):
    filename = os.path.basename(filepath)
    if filename in SKIP_FILES:
        print(f"  SKIPPED (custom content): {filepath}")
        return

    desc = DESCRIPTIONS.get(filename, "MRTKLIB module")

    with open(filepath, 'r') as f:
        content = f.read()

    comment, rest, is_doxygen = extract_first_comment(content)
    license_header = HEADER_TEMPLATE.format(filename=filename, description=desc)

    if is_doxygen:
        # INSERT license header ABOVE the existing Doxygen block
        new_content = license_header + comment + '\n' + rest
    else:
        # REPLACE the traditional header with license header
        # Strip leading blank lines from rest
        rest = rest.lstrip('\n')
        new_content = license_header + rest

    # Ensure file ends with exactly one newline
    new_content = new_content.rstrip('\n') + '\n'

    with open(filepath, 'w') as f:
        f.write(new_content)

    action = "preserved Doxygen" if is_doxygen else "replaced header"
    print(f"  {action}: {filepath}")


def collect_files():
    files = []

    # Source subdirectories (all .c files)
    src_subdirs = [
        os.path.join(PROJ, "src", "core"),
        os.path.join(PROJ, "src", "data"),
        os.path.join(PROJ, "src", "data", "rcv"),
        os.path.join(PROJ, "src", "models"),
        os.path.join(PROJ, "src", "rtcm"),
        os.path.join(PROJ, "src", "stream"),
        os.path.join(PROJ, "src", "madoca"),
        os.path.join(PROJ, "src", "pos"),
        os.path.join(PROJ, "src", "clas"),
    ]
    for d in src_subdirs:
        for f in sorted(os.listdir(d)):
            if f.startswith("mrtk_") and f.endswith(".c"):
                files.append(os.path.join(d, f))

    # include/mrtklib/*.h
    inc_dir = os.path.join(PROJ, "include", "mrtklib")
    for f in sorted(os.listdir(inc_dir)):
        if (f.startswith("mrtk_") and f.endswith(".h")) or f in ("mrtklib.h", "rtklib.h"):
            files.append(os.path.join(inc_dir, f))

    return files


def main():
    files = collect_files()
    print(f"Processing {len(files)} files...")
    for filepath in files:
        process_file(filepath)

    skipped = sum(1 for f in files if os.path.basename(f) in SKIP_FILES)
    processed = len(files) - skipped
    print(f"\nDone. {processed} files updated, {skipped} skipped (custom content).")
    if skipped:
        print("Skipped files need manual copyright line insertion:")
        for f in files:
            if os.path.basename(f) in SKIP_FILES:
                print(f"  {f}")


if __name__ == "__main__":
    main()
