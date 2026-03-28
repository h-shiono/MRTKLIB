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


# ═══════════════════════════════════════════════════════════════════════════════
# Option metadata: description and applicable positioning modes
#
# Modes abbreviations:
#   All      — All positioning modes
#   SPP      — Single Point Positioning
#   DGPS     — Differential GPS
#   RTK      — Kinematic / Static / Moving-base / Fixed
#   PPP      — PPP-Kinematic / PPP-Static / PPP-Fixed
#   PPP-RTK  — CLAS PPP-RTK
#   SSR2OSR  — SSR to OSR conversion
#   VRS      — VRS-RTK
#   RT       — Real-time only (rtkrcv / mrtk run)
#   PP       — Post-processing only (rnx2rtkp / mrtk post)
# ═══════════════════════════════════════════════════════════════════════════════

# {legacy_key: (modes, description)}
# Description for enum types should NOT include the values — they are appended
# automatically from _ENUM_STRINGS.
_OPTION_META: dict[str, tuple[str, str]] = {
    # ── positioning ──────────────────────────────────────────────────────────
    "pos1-posmode":    ("All", "Positioning mode selector."),
    "pos1-frequency":  ("All", "Number of carrier frequencies to use. CLAS PPP-RTK requires `l1+2` (nf=2)."),
    "pos1-soltype":    ("PP", "Filter direction for post-processing."),
    "pos1-elmask":     ("All", "Minimum satellite elevation angle (degrees). Satellites below this are excluded."),
    "pos1-dynamics":   ("All", "Enable receiver dynamics model (velocity/acceleration state estimation)."),
    "pos1-sateph":     ("All", "Satellite ephemeris source. SSR modes (`brdc+ssrapc`, `brdc+ssrcom`) are used for PPP and PPP-RTK."),
    "pos1-navsys":     ("All", "GNSS constellations to use. Accepts a human-readable string list like `[\"GPS\", \"Galileo\"]`."),
    "pos1-exclsats":   ("All", "Satellites to exclude. Space-separated PRN list (e.g., `G01 G02`). Prefix `+` to include only."),
    "pos1-signals":    ("All", "Explicit signal code list. Overrides default observation definition when set."),

    # ── positioning.clas ─────────────────────────────────────────────────────
    "pos1-gridsel":    ("PPP-RTK, VRS", "CLAS grid search radius (m). Controls how far from the rover to search for grid-based tropospheric/ionospheric corrections."),
    "pos1-rectype":    ("PPP-RTK, VRS", "Rover receiver type identifier. Used for ISB (inter-system bias) table lookup."),
    "pos1-rux":        ("PPP-RTK, VRS", "Rover approximate position X in ECEF (m). Used for initial CLAS grid search before first fix."),
    "pos1-ruy":        ("PPP-RTK, VRS", "Rover approximate position Y in ECEF (m)."),
    "pos1-ruz":        ("PPP-RTK, VRS", "Rover approximate position Z in ECEF (m)."),

    # ── positioning.snr_mask ─────────────────────────────────────────────────
    "pos1-snrmask_r":  ("All", "Enable elevation-dependent SNR mask for the rover receiver."),
    "pos1-snrmask_b":  ("RTK", "Enable elevation-dependent SNR mask for the base station."),
    "pos1-snrmask_L1": ("All", "L1 SNR mask values per elevation bin (dBHz)."),
    "pos1-snrmask_L2": ("All", "L2 SNR mask values per elevation bin (dBHz)."),
    "pos1-snrmask_L5": ("All", "L5 SNR mask values per elevation bin (dBHz)."),

    # ── positioning.corrections ──────────────────────────────────────────────
    "pos1-posopt1":    ("All", "Apply satellite antenna phase center offset correction using ANTEX file."),
    "pos1-posopt2":    ("All", "Apply receiver antenna phase center offset/variation correction."),
    "pos1-posopt3":    ("PPP, PPP-RTK, VRS", "Phase wind-up correction."),
    "pos1-posopt4":    ("PPP, PPP-RTK", "Exclude satellites in eclipse (yaw maneuver period) to avoid degraded orbit/clock."),
    "pos1-posopt5":    ("SPP", "RAIM fault detection and exclusion for single-point positioning."),
    "pos1-posopt6":    ("PPP-RTK, VRS, SSR2OSR", "Ionospheric compensation method for SSR-based processing."),
    "pos1-posopt7":    ("PPP-RTK, VRS", "Enable partial ambiguity resolution (fix a satellite subset)."),
    "pos1-posopt8":    ("PPP, PPP-RTK, VRS", "Apply relativistic Shapiro time delay correction."),
    "pos1-posopt9":    ("PPP-RTK, VRS", "Exclude QZS satellites from reference satellite selection in DD processing."),
    "pos1-posopt10":   ("PPP-RTK, VRS", "Disable phase bias adjustment. Used when phase bias is already applied by SSR corrections."),
    "pos1-posopt11":   ("PPP-RTK, VRS", "GPS frequency pair selection for CLAS processing."),
    "pos1-posopt12":   ("—", "Reserved for future use."),
    "pos1-posopt13":   ("PPP-RTK, VRS", "QZS frequency pair selection for CLAS processing."),
    "pos1-tidecorr":   ("PPP, PPP-RTK, VRS", "Tidal displacement correction. Option `solid+otl-clasgrid+pole` uses CLAS grid-based ocean tide loading."),

    # ── positioning.atmosphere ───────────────────────────────────────────────
    "pos1-ionoopt":    ("All", "Ionospheric correction model. PPP uses `dual-freq` or `est-stec`. PPP-RTK uses `est-stec` or `est-adaptive`. RTK typically uses `dual-freq`."),
    "pos1-tropopt":    ("All", "Tropospheric correction model. PPP/PPP-RTK use `est-ztd` or `est-ztdgrad`. SPP/RTK typically use `saas`."),

    # ── ambiguity_resolution ─────────────────────────────────────────────────
    "pos2-armode":     ("RTK, PPP, PPP-RTK, VRS", "Ambiguity resolution strategy."),
    "pos2-gpsarmode":  ("RTK", "GPS AR mode for GLONASS fix-and-hold second pass."),
    "pos2-gloarmode":  ("RTK", "GLONASS ambiguity resolution mode."),
    "pos2-bdsarmode":  ("RTK, PPP-RTK", "BDS ambiguity resolution enable/disable."),
    "pos2-qzsarmode":  ("PPP-RTK, VRS", "QZS ambiguity resolution mode."),
    "pos2-arsys":      ("RTK, PPP-RTK, VRS", "Constellation bitmask for AR. Limits which systems participate in integer ambiguity resolution."),

    # ── ambiguity_resolution.thresholds ──────────────────────────────────────
    "pos2-arthres":    ("RTK, PPP, PPP-RTK, VRS", "LAMBDA ratio test threshold (2nd-best / best). Typical value: 3.0."),
    "pos2-arthres1":   ("RTK, PPP, PPP-RTK", "Secondary AR threshold. In MADOCA-PPP: max 3D position std-dev to start narrow-lane AR."),
    "pos2-arthres2":   ("RTK, PPP-RTK", "Additional AR threshold parameter."),
    "pos2-arthres3":   ("RTK, PPP-RTK", "Additional AR threshold parameter."),
    "pos2-arthres4":   ("RTK", "Additional AR threshold parameter."),
    "pos2-arthres5":   ("PPP-RTK", "Chi-square threshold for hold validation."),
    "pos2-arthres6":   ("PPP-RTK", "Chi-square threshold for fix validation."),
    "pos2-aralpha":    ("PPP-RTK, VRS", "AR significance level (ILS success rate)."),
    "pos2-arelmask":   ("RTK, PPP-RTK, VRS", "Minimum satellite elevation for AR participation (degrees)."),
    "pos2-elmaskhold": ("RTK, PPP-RTK, VRS", "Minimum satellite elevation for fix-and-hold constraint application (degrees)."),

    # ── ambiguity_resolution.counters ────────────────────────────────────────
    "pos2-arlockcnt":  ("RTK, PPP-RTK, VRS", "Minimum continuous lock count before a satellite participates in AR."),
    "pos2-arminfix":   ("RTK, PPP-RTK", "Minimum fix epochs before applying fix-and-hold constraint."),
    "pos2-armaxiter":  ("RTK, PPP-RTK", "Maximum LAMBDA search iterations per epoch."),
    "pos2-aroutcnt":   ("RTK, PPP-RTK, VRS", "Reset ambiguity after this many continuous outage epochs."),

    # ── ambiguity_resolution.partial_ar ──────────────────────────────────────
    "pos2-arminamb":       ("PPP-RTK, VRS", "Minimum number of ambiguities required for partial AR attempt."),
    "pos2-armaxdelsat":    ("PPP-RTK, VRS", "Maximum satellites to exclude during partial AR satellite rotation."),
    "pos2-arminfixsats":   ("RTK, PPP-RTK", "Minimum DD pairs required for a valid fix. 0 = disabled."),
    "pos2-armindropsats":  ("RTK", "Minimum DD pairs before excluding the weakest satellite in partial AR. 0 = disabled."),
    "pos2-arminholdsats":  ("RTK", "Minimum DD pairs for fix-and-hold mode activation. 0 = no minimum."),
    "pos2-arfilter":       ("RTK, PPP-RTK", "Exclude newly-locked satellites that degrade the AR ratio."),

    # ── ambiguity_resolution.hold ────────────────────────────────────────────
    "pos2-varholdamb":  ("PPP-RTK, VRS", "Variance of held ambiguity pseudo-observation (cyc\u00b2). Controls how tightly held ambiguities constrain the filter."),
    "pos2-gainholdamb": ("RTK", "Gain factor for fractional GLONASS/SBAS inter-channel bias update in fix-and-hold."),

    # ── rejection ────────────────────────────────────────────────────────────
    "pos2-rejionno":   ("RTK, PPP", "Innovation (pre-fit residual) threshold (m). Observations exceeding this are rejected."),
    "pos2-rejionno1":  ("PPP-RTK, VRS", "L1/L2 residual rejection threshold (\u03c3)."),
    "pos2-rejionno2":  ("PPP-RTK, VRS", "Dispersive (ionospheric) residual rejection threshold (\u03c3)."),
    "pos2-rejionno3":  ("PPP-RTK, VRS", "Non-dispersive (geometric) residual rejection threshold (\u03c3)."),
    "pos2-rejionno4":  ("PPP-RTK, VRS", "Chi-square threshold for hold-mode outlier detection."),
    "pos2-rejionno5":  ("PPP-RTK, VRS", "Chi-square threshold for fix-mode outlier detection."),
    "pos2-rejgdop":    ("All", "Maximum GDOP for valid solution output."),
    "pos2-rejdiffpse": ("VRS", "Pseudorange consistency check threshold (m). Rejects satellites with large code-phase disagreement."),
    "pos2-poserrcnt":  ("VRS", "Number of consecutive position error epochs before filter reset."),

    # ── slip_detection ───────────────────────────────────────────────────────
    "pos2-slipthres":  ("RTK, PPP, PPP-RTK, VRS", "Geometry-free (LG) cycle slip detection threshold (m)."),
    "pos2-thresdop":   ("RTK, PPP, PPP-RTK", "Doppler-phase rate cycle slip detection threshold (cyc/s). 0 = disabled."),

    # ── kalman_filter ────────────────────────────────────────────────────────
    "pos2-niter":      ("RTK, PPP, PPP-RTK, VRS", "Number of measurement update iterations per epoch. More iterations improve linearization accuracy."),
    "pos2-syncsol":    ("RT", "Synchronize solution output with observation time in real-time mode."),

    # ── kalman_filter.measurement_error ──────────────────────────────────────
    "stats-eratio1":    ("All", "Code/phase measurement error ratio for L1. Code error = phase error \u00d7 ratio. Typical: 100."),
    "stats-eratio2":    ("All", "Code/phase measurement error ratio for L2."),
    "stats-eratio3":    ("All", "Code/phase measurement error ratio for L5."),
    "stats-errphase":   ("All", "Base carrier phase measurement error (m). Used in elevation-dependent weighting: error = phase + phase_elevation / sin(el)."),
    "stats-errphaseel": ("All", "Elevation-dependent carrier phase error coefficient (m)."),
    "stats-errphasebl": ("RTK", "Baseline-length-dependent phase error (m/10km). Proportional to rover\u2013base distance."),
    "stats-errdoppler": ("All", "Doppler measurement error (Hz)."),
    "stats-uraratio":   ("PPP", "User Range Accuracy scaling ratio. Adjusts satellite-specific weighting based on broadcast URA."),

    # ── kalman_filter.initial_std ────────────────────────────────────────────
    "stats-stdbias":   ("RTK, PPP, PPP-RTK, VRS", "Initial standard deviation for carrier phase bias states (m)."),
    "stats-stdiono":   ("PPP-RTK, VRS", "Initial standard deviation for ionospheric delay states (m)."),
    "stats-stdtrop":   ("PPP, PPP-RTK, VRS", "Initial standard deviation for tropospheric delay states (m)."),

    # ── kalman_filter.process_noise ──────────────────────────────────────────
    "stats-prnbias":      ("RTK, PPP, PPP-RTK, VRS", "Process noise for carrier phase bias states (m/\u221as)."),
    "stats-prniono":      ("PPP-RTK, VRS", "Process noise for ionospheric delay states (m/\u221as)."),
    "stats-prnionomax":   ("PPP-RTK, VRS", "Maximum ionospheric process noise clamp (m). Prevents excessive iono state growth."),
    "stats-prntrop":      ("PPP, PPP-RTK, VRS", "Process noise for tropospheric delay states (m/\u221as)."),
    "stats-prnaccelh":    ("All", "Horizontal acceleration process noise (m/s\u00b2). Active when `dynamics = true`."),
    "stats-prnaccelv":    ("All", "Vertical acceleration process noise (m/s\u00b2). Active when `dynamics = true`."),
    "stats-prnposith":    ("PPP-RTK, VRS", "Horizontal position process noise (m)."),
    "stats-prnpositv":    ("PPP-RTK, VRS", "Vertical position process noise (m)."),
    "stats-prnpos":       ("All", "General position process noise (m). Fallback when h/v not specified."),
    "stats-prnifb":       ("PPP", "Inter-frequency bias process noise (m). For multi-frequency PPP bias estimation."),
    "stats-prndcb":       ("PPP", "Alias for `ifb` (legacy name)."),
    "stats-tconstiono":   ("PPP-RTK, VRS", "Ionospheric time constant (s). Controls iono state temporal correlation in the adaptive filter."),
    "stats-clkstab":      ("PPP", "Receiver clock stability (s/s). Used in PPP clock state prediction."),

    # ── adaptive_filter ──────────────────────────────────────────────────────
    "pos2-prnadpt":    ("PPP-RTK, VRS", "Enable adaptive Kalman filter process noise scaling."),
    "pos2-forgetion":  ("PPP-RTK, VRS", "Forgetting factor for ionospheric state (0\u20131). Lower = faster adaptation."),
    "pos2-afgainion":  ("PPP-RTK, VRS", "Adaptive gain for ionospheric process noise adjustment."),
    "pos2-forgetpva":  ("PPP-RTK, VRS", "Forgetting factor for position/velocity/acceleration states (0\u20131)."),
    "pos2-afgainpva":  ("PPP-RTK, VRS", "Adaptive gain for PVA process noise adjustment."),

    # ── signals ──────────────────────────────────────────────────────────────
    "pos2-siggps":     ("PPP, PPP-RTK", "GPS frequency pair selection for PPP/PPP-RTK processing."),
    "pos2-sigqzs":     ("PPP, PPP-RTK", "QZS frequency pair selection."),
    "pos2-siggal":     ("PPP, PPP-RTK", "Galileo frequency pair selection."),
    "pos2-sigbds2":    ("PPP, PPP-RTK", "BDS-2 frequency pair selection."),
    "pos2-sigbds3":    ("PPP, PPP-RTK", "BDS-3 frequency pair selection."),

    # ── receiver ─────────────────────────────────────────────────────────────
    "pos2-ionocorr":   ("PPP", "Enable ionospheric correction in MADOCA-PPP processing."),
    "pos2-ign_chierr": ("SPP", "Ignore chi-square test errors in SPP solution validation."),
    "pos2-bds2bias":   ("PPP", "Enable BDS-2 code bias correction (satellite-dependent group delay)."),
    "pos2-pppsatcb":   ("PPP", "PPP satellite code bias source selection."),
    "pos2-pppsatpb":   ("PPP", "PPP satellite phase bias source selection."),
    "pos2-uncorrbias": ("PPP", "Uncorrelated bias parameter for PPP processing."),
    "pos2-maxbiasdt":  ("PPP", "Maximum age of bias correction data (s) before invalidation."),
    "pos2-sattmode":   ("PPP", "Satellite processing mode selector."),
    "pos2-phasshft":   ("PPP-RTK, VRS", "Phase cycle shift correction. Corrects quarter-cycle shifts between systems."),
    "pos2-isb":        ("PPP-RTK, VRS", "Inter-system bias estimation mode."),
    "pos2-rectype":    ("PPP-RTK, VRS", "Reference station receiver type for ISB table lookup."),
    "pos2-maxage":     ("RTK, PPP-RTK", "Maximum age of differential correction (s)."),
    "pos2-baselen":    ("RTK", "Baseline length constraint (m). 0 = no constraint."),
    "pos2-basesig":    ("RTK", "Standard deviation of baseline length constraint (m)."),

    # ── antenna.rover ────────────────────────────────────────────────────────
    "ant1-postype":    ("All", "Rover position input type."),
    "ant1-pos1":       ("All", "Rover position coordinate 1 (latitude or X, depending on `position_type`)."),
    "ant1-pos2":       ("All", "Rover position coordinate 2 (longitude or Y)."),
    "ant1-pos3":       ("All", "Rover position coordinate 3 (height or Z)."),
    "ant1-anttype":    ("All", "Rover antenna type (must match ANTEX file entry). `*` = use RINEX header."),
    "ant1-antdele":    ("All", "Rover antenna delta East offset (m)."),
    "ant1-antdeln":    ("All", "Rover antenna delta North offset (m)."),
    "ant1-antdelu":    ("All", "Rover antenna delta Up offset (m)."),

    # ── antenna.base ─────────────────────────────────────────────────────────
    "ant2-postype":    ("RTK, VRS", "Base station position input type."),
    "ant2-pos1":       ("RTK, VRS", "Base position coordinate 1."),
    "ant2-pos2":       ("RTK, VRS", "Base position coordinate 2."),
    "ant2-pos3":       ("RTK, VRS", "Base position coordinate 3."),
    "ant2-anttype":    ("RTK, VRS", "Base antenna type."),
    "ant2-antdele":    ("RTK, VRS", "Base antenna delta East offset (m)."),
    "ant2-antdeln":    ("RTK, VRS", "Base antenna delta North offset (m)."),
    "ant2-antdelu":    ("RTK, VRS", "Base antenna delta Up offset (m)."),
    "ant2-maxaveep":   ("RT", "Maximum epochs for base position averaging in real-time mode."),
    "ant2-initrst":    ("RT", "Reset base position averaging on restart."),

    # ── output ───────────────────────────────────────────────────────────────
    "out-solformat":   ("All", "Solution output format."),
    "out-outhead":     ("All", "Output header line with option summary."),
    "out-outopt":      ("All", "Output processing options in header."),
    "out-outvel":      ("All", "Include velocity in solution output."),
    "out-timesys":     ("All", "Time system for output."),
    "out-timeform":    ("All", "Time format."),
    "out-timendec":    ("All", "Number of decimal digits for time output."),
    "out-degform":     ("All", "Coordinate format."),
    "out-fieldsep":    ("All", "Field separator character for output."),
    "out-outsingle":   ("All", "Output SPP solution alongside fixed solution."),
    "out-maxsolstd":   ("All", "Maximum solution standard deviation for output (m). Solutions exceeding this are suppressed."),
    "out-height":      ("All", "Height output type."),
    "out-geoid":       ("All", "Geoid model for geodetic height conversion."),
    "out-solstatic":   ("PP", "Static mode output control."),
    "out-nmeaintv1":   ("All", "NMEA GGA/RMC output interval (s)."),
    "out-nmeaintv2":   ("All", "NMEA GSA/GSV output interval (s)."),
    "out-outstat":     ("All", "Solution status output level."),

    # ── files ────────────────────────────────────────────────────────────────
    "file-satantfile":   ("All", "Satellite antenna ANTEX file. Required for satellite antenna PCV correction."),
    "file-rcvantfile":   ("All", "Receiver antenna ANTEX file. Required for receiver antenna PCV correction."),
    "file-staposfile":   ("All", "Station position file for fixed/known positions."),
    "file-geoidfile":    ("All", "Geoid data file for geodetic height conversion."),
    "file-ionofile":     ("All", "IONEX ionosphere map file. Used when `ionosphere = ionex-tec`."),
    "file-dcbfile":      ("PPP", "Differential code bias file."),
    "file-eopfile":      ("PPP, PPP-RTK", "Earth orientation parameter file for precise coordinate frame."),
    "file-blqfile":      ("PPP, PPP-RTK", "BLQ ocean tide loading file. Required when `tidal_correction` includes OTL."),
    "file-elmaskfile":   ("All", "Azimuth-dependent elevation mask file."),
    "file-tempdir":      ("All", "Temporary directory for intermediate files."),
    "file-geexefile":    ("All", "Google Earth executable path (legacy GUI feature)."),
    "file-solstatfile":  ("All", "Solution statistics output file path."),
    "file-tracefile":    ("All", "Debug trace output file path."),
    "file-fcbfile":      ("PPP", "Fractional cycle bias file for PPP-AR."),
    "file-biafile":      ("PPP", "Bias SINEX file for PPP satellite bias correction."),
    "file-cssrgridfile": ("PPP-RTK, VRS", "CLAS CSSR grid definition file. Required for CLAS grid-based corrections."),
    "file-isbfile":      ("PPP-RTK, VRS", "Inter-system bias correction table file."),
    "file-phacycfile":   ("PPP-RTK, VRS", "Phase cycle shift correction table file."),
    "file-cmdfile1":     ("RT", "Receiver command file for input stream 1."),
    "file-cmdfile2":     ("RT", "Receiver command file for input stream 2."),
    "file-cmdfile3":     ("RT", "Receiver command file for input stream 3."),

    # ── server ───────────────────────────────────────────────────────────────
    "misc-svrcycle":    ("RT", "Server main loop cycle interval (ms). Controls processing rate."),
    "misc-timeout":     ("RT", "Stream read timeout (ms). After timeout, stream is considered disconnected."),
    "misc-reconnect":   ("RT", "Stream reconnection interval (ms) after disconnect."),
    "misc-nmeacycle":   ("RT", "NMEA request transmission cycle (ms) for NTRIP caster position updates."),
    "misc-buffsize":    ("RT", "Stream input buffer size (bytes)."),
    "misc-navmsgsel":   ("RT", "Navigation message type selection for multi-source environments."),
    "misc-proxyaddr":   ("RT", "HTTP proxy address for NTRIP connections."),
    "misc-fswapmargin": ("RT", "File swap margin (s) for continuous logging across file boundaries."),
    "misc-timeinterp":  ("RT", "Enable time interpolation between observation epochs."),
    "misc-sbasatsel":   ("RT", "SBAS satellite selection."),
    "misc-maxobsloss":  ("RT", "Maximum observation gap duration before filter reset (s)."),
    "misc-floatcnt":    ("RT", "Number of float epochs before triggering filter reset."),
    "misc-rnxopt1":     ("All", "RINEX conversion option string for rover stream."),
    "misc-rnxopt2":     ("All", "RINEX conversion option string for base stream."),
    "misc-pppopt":      ("PPP", "PPP processing option string (passed to PPP engine)."),
    "misc-rtcmopt":     ("RT", "RTCM decoder option string."),
    "misc-l6mrg":       ("RT", "L6 message margin (epochs) for CLAS L6 real-time synchronization."),
    "misc-regularly":   ("VRS", "Regular filter reset interval (s). 0 = disabled."),
    "misc-startcmd":    ("RT", "Shell command executed on server start."),
    "misc-stopcmd":     ("RT", "Shell command executed on server stop."),
}


def get_enum_values(legacy_key: str) -> str:
    """Get enum values string for a legacy key, if available."""
    if legacy_key not in _KEY_ENUM:
        return ""
    enum_name = _KEY_ENUM[legacy_key]
    if enum_name not in _ENUM_STRINGS:
        return ""
    return _ENUM_STRINGS[enum_name]


def format_enum_values(raw: str) -> str:
    """Format raw enum string into readable middle-dot-separated list without numeric prefixes.

    Input:  "0:single,1:dgps,2:kinematic"
    Output: "`single` · `dgps` · `kinematic`"
    """
    if not raw:
        return ""
    parts = []
    for item in raw.split(","):
        _, _, name = item.partition(":")
        if name:
            parts.append(f"`{name}`")
    return " · ".join(parts)


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
    lines.append("TOML configuration options available in MRTKLIB.")
    lines.append("Options are grouped by their TOML section.")
    lines.append("")
    lines.append("!!! tip \"Auto-generated\"")
    lines.append("    This page is auto-generated from the internal option mapping table.")
    lines.append("    Run `python scripts/docs/gen_config_ref.py > docs/reference/config-options.md` to regenerate.")
    lines.append("")

    # Mode abbreviation legend
    lines.append("## Mode Abbreviations")
    lines.append("")
    lines.append("| Abbreviation | Positioning Mode(s) |")
    lines.append("|:-------------|:--------------------|")
    lines.append("| **All** | All modes |")
    lines.append("| **SPP** | Single Point Positioning (`single`) |")
    lines.append("| **DGPS** | Differential GPS (`dgps`) |")
    lines.append("| **RTK** | `kinematic` · `static` · `movingbase` · `fixed` |")
    lines.append("| **PPP** | `ppp-kine` · `ppp-static` · `ppp-fixed` |")
    lines.append("| **PPP-RTK** | `ppp-rtk` (CLAS PPP-RTK) |")
    lines.append("| **SSR2OSR** | `ssr2osr` · `ssr2osr-fixed` |")
    lines.append("| **VRS** | `vrs-rtk` |")
    lines.append("| **RT** | Real-time only (`mrtk run` / `rtkrcv`) |")
    lines.append("| **PP** | Post-processing only (`mrtk post` / `rnx2rtkp`) |")
    lines.append("")

    # Generate each section
    for section, entries in sections.items():
        display = section_names.get(section, section)

        lines.append(f"## {display}")
        lines.append("")
        lines.append(f"TOML section: `[{section}]`")
        lines.append("")
        lines.append("| TOML Key | Type | Modes | Description |")
        lines.append("|:---------|:-----|:------|:------------|")

        seen_keys: set[str] = set()
        for legacy_key, _, toml_key, vtype in entries:
            # Skip duplicate TOML keys (e.g. aliases like stats-prndcb → ifb)
            if toml_key in seen_keys:
                continue
            seen_keys.add(toml_key)

            tl = type_label(vtype)

            # Get description and modes from metadata
            modes, desc = _OPTION_META.get(legacy_key, ("", ""))

            # Build description with enum values / type hints
            if vtype == "bool":
                pass  # No enum values needed for boolean
            elif vtype == "navsys":
                if '["' not in desc:
                    desc += ' e.g. `["GPS", "Galileo", "QZSS"]`'
            elif vtype == "siglist":
                if '["' not in desc:
                    desc += ' e.g. `["G1C", "G2W", "E1C"]`'
            elif vtype == "snr":
                if "9-element" not in desc:
                    desc += " 9-element array (0–45 dBHz per 5° elevation bin)."
            else:
                enum_vals = get_enum_values(legacy_key)
                if enum_vals:
                    formatted = format_enum_values(enum_vals)
                    desc += f" {formatted}"

            lines.append(f"| `{toml_key}` | {tl} | {modes} | {desc} |")

        lines.append("")

        # Section-specific supplementary notes
        if section == "positioning":
            lines.append("### Frequency Index Mapping")
            lines.append("")
            lines.append("The `frequency` option selects how many frequency slots to use (`l1` = 1, `l1+2` = 2, etc.).")
            lines.append("Each slot maps to a different signal depending on the constellation:")
            lines.append("")
            lines.append("| | L1 (idx 0) | L2 (idx 1) | L3 (idx 2) | L4 (idx 3) | L5 (idx 4) |")
            lines.append("|:---|:-----------|:-----------|:-----------|:-----------|:-----------|")
            lines.append("| **GPS** | L1 | L2 | L5 | — | — |")
            lines.append("| **GLONASS** | G1 | G2 | G3 | — | — |")
            lines.append("| **Galileo** | E1 | E5a | E5b | E6 | E5a+b |")
            lines.append("| **QZSS** | L1 | L5 | L2 | L6 | — |")
            lines.append("| **BDS** | B1I/B1C/B1A | B3I/B3A | B2I/B2b | B2a | B2a+b |")
            lines.append("| **SBAS** | L1 | L5 | — | — | — |")
            lines.append("| **NavIC** | L5 | S | — | — | — |")
            lines.append("")
            lines.append("!!! warning \"CLAS PPP-RTK: Use `l1+2` (nf=2)\"")
            lines.append("    With `l1+2`, GPS uses L1+L2 and Galileo uses E1+E5a.")
            lines.append("    CLAS does not provide E5b bias corrections. Using `l1+2+3` (nf=3)")
            lines.append("    adds the E5b slot without valid bias, causing false cycle slips on")
            lines.append("    Galileo and degrading fix rate from >99% to ~67%.")
            lines.append("")

    # Stream configuration (not in MAPPING)
    lines.append("## Streams")
    lines.append("")
    lines.append("TOML section: `[streams]` — **Real-time only** (`mrtk run` / `rtkrcv`)")
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
    lines.append('format = "clas"')
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
    lines.append("| Key | Type | Description |")
    lines.append("|:----|:-----|:------------|")
    lines.append("| `type` | string | Stream type: `serial` · `file` · `tcpsvr` · `tcpcli` · `ntrip` · `off` |")
    lines.append("| `path` | string | Stream path (serial device, file path, or URL) |")
    lines.append("| `format` | string | Data format: `rtcm3` · `ubx` · `sbf` · `binex` · `rinex` · `clas` · `l6e` |")
    lines.append("| `nmeareq` | boolean | Send NMEA position request to stream |")
    lines.append("| `nmealat` | float | NMEA request latitude (degrees) |")
    lines.append("| `nmealon` | float | NMEA request longitude (degrees) |")
    lines.append("")

    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
