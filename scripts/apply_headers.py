#!/usr/bin/env python3
"""Apply unified license headers to all mrtk_* files.

Strategy:
  - Doxygen blocks (/** ... */): INSERT license header ABOVE, preserve Doxygen.
  - Traditional blocks (/* ... */): REPLACE with license header.
"""
import os
import re
import sys

PROJ = "/Volumes/SDSSDX3N-2T00-G26/dev/MRTKLIB"

HEADER_TEMPLATE = """\
/*------------------------------------------------------------------------------
 * {filename} : {description}
 *
 * Copyright (C) 2026 H.SHIONO (MRTKLIB Project)
 * Copyright (C) 2023-2025 Japan Aerospace Exploration Agency
 * Copyright (C) 2023-2025 TOSHIBA ELECTRONIC TECHNOLOGIES CORPORATION
 * Copyright (C) 2014 T.SUZUKI
 * Copyright (C) 2007-2023 T.TAKASU
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *----------------------------------------------------------------------------*/
"""

# Map of filename -> one-line description
DESCRIPTIONS = {
    # --- src/ .c files ---
    "mrtk_core.c":             "MRTKLIB core context management and logging",
    "mrtk_time.c":             "time and date functions",
    "mrtk_mat.c":              "matrix and vector functions",
    "mrtk_coords.c":           "coordinate transformation functions",
    "mrtk_atmos.c":            "atmosphere model functions",
    "mrtk_eph.c":              "satellite ephemeris and clock functions",
    "mrtk_peph.c":             "precise ephemeris and earth rotation parameter functions",
    "mrtk_bits.c":             "bit manipulation, CRC, and word decode functions",
    "mrtk_sys.c":              "system utility functions",
    "mrtk_astro.c":            "astronomical functions for sun/moon position",
    "mrtk_antenna.c":          "antenna model and PCV functions",
    "mrtk_station.c":          "station position and BLQ functions",
    "mrtk_obs.c":              "observation data functions",
    "mrtk_nav.c":              "navigation data functions",
    "mrtk_rinex.c":            "RINEX file I/O functions",
    "mrtk_tides.c":            "tide displacement correction functions",
    "mrtk_geoid.c":            "geoid model functions",
    "mrtk_sbas.c":             "SBAS message decoding and correction functions",
    "mrtk_rtcm.c":             "RTCM common functions",
    "mrtk_rtcm2.c":            "RTCM version 2 message functions",
    "mrtk_rtcm3.c":            "RTCM version 3 message decoder functions",
    "mrtk_rtcm3e.c":           "RTCM version 3 message encoder functions",
    "mrtk_rtcm3lcl.c":         "RTCM3 local correction message encoder/decoder",
    "mrtk_ionex.c":            "IONEX TEC grid functions",
    "mrtk_opt.c":              "processing and solution option defaults",
    "mrtk_sol.c":              "solution I/O functions",
    "mrtk_bias_sinex.c":       "BIAS-SINEX file reader and bias functions",
    "mrtk_fcb.c":              "fractional cycle bias (FCB) functions",
    "mrtk_spp.c":              "standard single-point positioning",
    "mrtk_madoca_local_corr.c":"MADOCA local correction common functions",
    "mrtk_madoca_local_comb.c":"MADOCA local correction data combination",
    "mrtk_madoca.c":           "MADOCA-PPP processing functions",
    "mrtk_lambda.c":           "integer ambiguity resolution (LAMBDA/MLAMBDA)",
    "mrtk_ppp_ar.c":           "PPP ambiguity resolution functions",
    "mrtk_rtkpos.c":           "RTK positioning functions",
    "mrtk_ppp.c":              "precise point positioning functions",
    "mrtk_postpos.c":          "post-processing positioning functions",
    "mrtk_options.c":          "option string processing functions",
    "mrtk_rcvraw.c":           "receiver raw data functions",
    "mrtk_stream.c":           "stream I/O functions",
    "mrtk_rtksvr.c":           "real-time RTK server functions",
    "mrtk_trace.c":            "debug trace and logging functions",
    # --- src/rcv/ .c files ---
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
    # --- include/mrtklib/ .h files ---
    "mrtklib.h":               "MRTKLIB public API for context management and logging",
    "mrtk_foundation.h":       "common types, standard includes, and system constants",
    "mrtk_const.h":            "physical, navigation, and system constants",
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


def main():
    files = []

    # src/*.c
    src_dir = os.path.join(PROJ, "src")
    for f in sorted(os.listdir(src_dir)):
        if f.startswith("mrtk_") and f.endswith(".c"):
            files.append(os.path.join(src_dir, f))

    # src/rcv/*.c
    rcv_dir = os.path.join(PROJ, "src", "rcv")
    for f in sorted(os.listdir(rcv_dir)):
        if f.startswith("mrtk_") and f.endswith(".c"):
            files.append(os.path.join(rcv_dir, f))

    # include/mrtklib/*.h
    inc_dir = os.path.join(PROJ, "include", "mrtklib")
    for f in sorted(os.listdir(inc_dir)):
        if (f.startswith("mrtk_") and f.endswith(".h")) or f in ("mrtklib.h", "rtklib.h"):
            files.append(os.path.join(inc_dir, f))

    print(f"Processing {len(files)} files...")
    for filepath in files:
        process_file(filepath)

    print(f"\nDone. {len(files)} files updated.")


if __name__ == "__main__":
    main()
