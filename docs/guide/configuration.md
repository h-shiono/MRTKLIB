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
| `[ambiguity_resolution]` | AR mode, thresholds, GLONASS/BDS AR |
| `[kalman_filter]` | Process noise, measurement noise |
| `[output]` | Solution format, time format, output paths |
| `[streams]` | Real-time stream configuration (rtkrcv only) |
| `[files]` | Antenna, DCB, geoid, ionosphere files |
| `[server]` / `[console]` | Server and console options (rtkrcv) |

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

A `+` prefix means "include only this satellite" (whitelist mode).

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
