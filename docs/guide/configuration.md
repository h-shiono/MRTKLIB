# Configuration (TOML)

Since v0.5.0, MRTKLIB uses [TOML](https://toml.io/) for all configuration files, replacing the legacy RTKLIB `key=value` `.conf` format.

## File Structure

A TOML configuration file is organized into sections:

```toml
[positioning]
mode = "ppp-rtk"
elevation_mask = 15.0
systems = ["GPS", "Galileo", "QZSS"]

[positioning.corrections]
satellite_antenna = false
receiver_antenna = true
phase_windup = "on"
tidal_correction = "solid+otl-clasgrid+pole"

[positioning.atmosphere]
ionosphere = "est-adaptive"
troposphere = "off"

[ambiguity_resolution]
mode = "fix-and-hold"

[kalman_filter]
# ...

[output]
format = "llh"
# ...
```

## Key Sections

| Section | Description |
|---------|-------------|
| `[positioning]` | Mode, frequency, elevation mask, constellations |
| `[positioning.corrections]` | Antenna models, phase windup, tidal corrections |
| `[positioning.atmosphere]` | Ionosphere and troposphere models |
| `[positioning.snr_mask]` | SNR mask thresholds per frequency |
| `[positioning.clas]` | CLAS-specific settings (grid, receiver type) |
| `[positioning.clas.ambiguities]` / `[positioning.relative]` | CLAS ambiguity and relative-positioning settings |
| `[positioning.madoca]` / `[positioning.ppp]` | MADOCA and PPP bias-product settings |
| `[input.*]` | RINEX, RTCM, and SBAS decoder options |
| `[ambiguity_resolution]` | AR mode, thresholds, GLONASS/BDS AR |
| `[kalman_filter]` | Process noise, measurement noise |
| `[output]` | Solution format, time format, output paths |
| `[streams]` | Real-time stream configuration (rtkrcv only) |
| `[files]` | Antenna, DCB, geoid, ionosphere files |
| `[server]` / `[console]` | Real-time server runtime and console options (rtkrcv) |

## Constellation Selection

Use the `systems` string list for human-readable constellation selection:

```toml
[positioning]
systems = ["GPS", "Galileo", "QZSS"]
```

Supported constellation names (case-insensitive):

| Name | Aliases | Bitmask |
|------|---------|---------|
| GPS | G | 0x01 |
| SBAS | S | 0x02 |
| GLONASS | GLO, R | 0x04 |
| Galileo | GAL, E | 0x08 |
| QZSS | QZS, J | 0x10 |
| BeiDou | BDS, CMP, C | 0x20 |
| NavIC | IRNSS, IRN, I | 0x40 |

!!! note "Backward Compatibility"
    The legacy numeric `constellations` key is still supported:

    ```toml
    constellations = 25  # GPS(1) + Galileo(8) + QZSS(16) = 25
    ```

    When both `systems` and `constellations` are present, `systems` takes priority.

## Satellite Exclusion

Exclude specific satellites using a string list:

```toml
[positioning]
excluded_sats = ["G01", "G02", "+E05"]
```

A `+` prefix inverts the entry: that satellite is force-included, and is kept even
when it would otherwise be dropped as unhealthy or for an excessive ephemeris
variance (URA). Satellites absent from the list are unaffected — the list is not a
whitelist.

Force-inclusion is not absolute. A satellite whose ephemeris is unavailable is still
dropped, because that check precedes the exclusion list; `+` cannot conjure an orbit
that was never decoded.

The legacy space-separated string form (`excluded_sats = "G01 G02 +E05"`) is still
accepted.

## Bundled Configuration Files

MRTKLIB ships with ready-to-use configuration files for common use cases:

### MADOCALIB (PPP / PPP-AR)

| File | Mode |
|------|------|
| `conf/madocalib/rnx2rtkp.toml` | PPP (post-processing) |
| `conf/madocalib/rnx2rtkp_pppar.toml` | PPP-AR (post-processing) |
| `conf/madocalib/rnx2rtkp_pppar_iono.toml` | PPP-AR + Ionosphere (post-processing) |

### CLASLIB (PPP-RTK / VRS-RTK)

| File | Mode |
|------|------|
| `conf/claslib/rnx2rtkp.toml` | PPP-RTK single-channel (post-processing) |
| `conf/claslib/rnx2rtkp_vrs.toml` | VRS-RTK (post-processing) |
| `conf/claslib/rtkrcv.toml` | PPP-RTK single-channel (real-time) |
| `conf/claslib/rtkrcv_2ch.toml` | PPP-RTK dual-channel (real-time) |

### MALIB (General)

| File | Mode |
|------|------|
| `conf/malib/rnx2rtkp.toml` | General post-processing |
| `conf/malib/rtkrcv.toml` | General real-time |

### IGS Products (`correction = "igs"` / `"igs-rts"`)

| File | Mode |
|------|------|
| `conf/igs/rnx2rtkp_ppp.toml` | Float PPP, precise SP3/CLK files |
| `conf/igs/rnx2rtkp_igspppar.toml` | Integer PPP-AR, GPS+Galileo |
| `conf/igs/rnx2rtkp_igspppar_gps.toml` | Integer PPP-AR, GPS-only |
| `conf/igs/rnx2rtkp_igsrts.toml` | Float PPP, real-time RTCM-SSR stream |

#### Integer PPP-AR with IGS products

Resolving integer ambiguities from IGS precise products needs a satellite
**phase-bias** product and the **uncombined** measurement model:

```toml
[positioning]
correction = "igs"

[positioning.atmosphere]
ionosphere = "est-stec"     # uncombined -- per-frequency ambiguity states

[ambiguity_resolution]
mode = "continuous"

[files]
# COD MGEX IAR Bias-SINEX (carries L* phase + C* code OSB). Use bias_sinex=,
# NOT dcb= (the legacy CODE-DCB parser ignores Bias-SINEX files).
bias_sinex = "COD0MGXFIN_<yyyyddd>0000_01D_01D_OSB.BIA"
```

The float iono-free config (`ionosphere = "dual-freq"`) **cannot** resolve
integers — it carries a single ambiguity per satellite, not per frequency, so
`ambiguity_resolution` is a no-op there. Galileo (or more constellations) is
recommended: GPS-only often has too few satellites for reliable narrow-lane
fixing on short sessions, though the wide-lane still fixes.

The float solution itself is cross-validated against RTKLIB 2.4.3: on the
GEONET FUJISAWA dataset both reach ≈8.4 cm 3D (1σ) versus the GSI F5
coordinate — i.e. MRTKLIB's IGS float PPP shows no meaningful degradation
relative to RTKLIB. (RTKLIB has no integer PPP-AR, so the fixed solution has no
upstream counterpart to compare against.)

**Constellations.** Ambiguity resolution covers GPS, Galileo, QZSS and BeiDou
(BDS-2/BDS-3), selected with the `[ambiguity_resolution].systems` bitmask
(GPS=1, Galileo=8, QZSS=16, BeiDou=32). GLONASS is float-only (FDMA has no
integer AR). Whether a constellation actually fixes also depends on the OSB
product carrying phase biases for the **signals you process**: the COD MGEX OSB
provides GPS and Galileo phase biases on the nominal `l1+2` pair, but its QZSS
phase biases are L1/L5 only (no L2) and it carries no BeiDou phase biases —
so GPS+Galileo is the dependable combination with that product today.

## Migration from pre-v0.7.6 TOML layouts

v0.7.6 re-organized the section taxonomy (the `[receiver]` and `[server]`
grab-bag sections were dismantled, and `[adaptive_filter]` moved to
`[positioning.clas.adaptive_filter]`). Old paths still load: the loader applies
the value and prints a warning pointing at the new location, and any key it
does not recognize is warned about instead of silently ignored. The full
old→new table is in the
[v0.7.6 changelog entry](https://github.com/h-shiono/MRTKLIB/blob/main/CHANGELOG.md);
the aliases are planned for removal in a future minor release, so update
configs when convenient.

## Migration from .conf

Use the included conversion script to migrate legacy `.conf` files:

```bash
python scripts/tools/conf2toml.py old_config.conf -o new_config.toml
```

The script maps legacy `key=value` pairs to their TOML equivalents. After conversion, format the output with `taplo`:

```bash
taplo format new_config.toml
```

!!! warning "Legacy Format Support"
    While `loadopts()` still accepts `.conf` files at runtime, all bundled configurations
    and tests use TOML exclusively since v0.5.0.
    For legacy `.conf` files, use the [`support/v0.4.x`](https://github.com/h-shiono/MRTKLIB/tree/support/v0.4.x) branch.

## Full Reference

See [Configuration Options](../reference/config-options.md) for a complete list of all available options.
