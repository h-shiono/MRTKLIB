# First Run

This guide walks you through running your first GNSS positioning solution with MRTKLIB.

## Post-Processing (mrtk post)

Post-processing computes positions from recorded observation and navigation files.

### PPP (Precise Point Positioning)

```bash
mrtk post -k conf/madocalib/rnx2rtkp.toml \
  obs.obs nav.nav correction.l6
```

- `-k` — Configuration file (TOML format)
- Positional arguments — Observation file, navigation file, and optionally L6 correction file(s)

### PPP-RTK (CLAS)

```bash
mrtk post -k conf/claslib/rnx2rtkp.toml \
  obs.obs nav.nav clas.l6
```

### Output Format

Results are written in NMEA-like position format (`.pos` file):

```
% GPST            latitude(deg)  longitude(deg)  height(m)  Q  ns  ...
2019/08/27 00:01:00.000   39.13515051  141.13287141   117.427  1   8  ...
```

- **Q=1** — Fixed solution (ambiguities resolved)
- **Q=2** — Float solution
- **Q=5** — Single point positioning

## Real-Time Processing (mrtk run)

Real-time processing receives live GNSS data streams and computes positions continuously.

### CLAS PPP-RTK (Single Channel)

```bash
mrtk run -s -o conf/claslib/rtkrcv.toml
```

- `-s` — Start processing immediately
- `-o` — Configuration file

### CLAS PPP-RTK (Dual Channel)

```bash
mrtk run -s -o conf/claslib/rtkrcv_2ch.toml
```

!!! tip "Stream Configuration"
    Real-time stream inputs are configured in the TOML file under `[streams]`:

    - `inpstr1` — Rover receiver (BINEX, UBX, or RTCM3)
    - `inpstr2` — Base station or L6 ch2 (repurposed for CLAS dual-channel)
    - `inpstr3` — Correction stream (L6 ch1 or SSR)

## Other Subcommands

| Command | Description | Example |
|---------|-------------|---------|
| `mrtk relay` | Relay data streams | `mrtk relay -in ntrip://... -out tcpsvr://:2101` |
| `mrtk convert` | Convert raw data to RINEX | `mrtk convert raw.ubx -o output.obs` |
| `mrtk ssr2obs` | SSR to pseudo-observations | `mrtk ssr2obs -k conf/claslib/ssr2obs.toml ...` |
| `mrtk bias` | Estimate receiver biases | `mrtk bias -k conf/madocalib/recvbias.toml ...` |

## Next Steps

- [CLI Reference](../guide/cli.md) — Detailed subcommand documentation
- [Configuration Guide](../guide/configuration.md) — TOML configuration format
- [Configuration Options](../reference/config-options.md) — Full options reference
