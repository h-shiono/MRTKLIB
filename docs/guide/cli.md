# CLI Reference

MRTKLIB provides a single unified binary `mrtk` with subcommands (BusyBox/Git pattern).

```
mrtk [COMMAND] [OPTIONS]
```

Every subcommand accepts both the original short flags (`-k`, `-o`, `-ts`, …) and
GNU-style long aliases (`--config`, `--output`, `--start`, …). Run
`mrtk <subcommand> --help` to see the full set, including any
subcommand-specific flags. Two subcommands intentionally do *not* bind `-h` to
help, because `-h` was already reserved for an existing flag — see the notes on
[`mrtk post`](#mrtk-post) and [`mrtk convert`](#mrtk-convert).

## Core Commands

### mrtk post

Run post-processing positioning (formerly `rnx2rtkp`).

```bash
mrtk post [options] obsfile navfile [navfile...] [l6file...]
```

| Option | Description |
|--------|-------------|
| `-k`, `--config FILE` | Configuration file (TOML) |
| `-o`, `--output FILE` | Output position file |
| `-ts`, `--start Y/M/D H:M:S` | Start time |
| `-te`, `--end Y/M/D H:M:S` | End time |
| `-ti`, `--interval SEC` | Processing interval (seconds) |
| `-p MODE` | Positioning mode (0:single, 1:dgps, …, 9:ppp-rtk) |
| `-f`, `--freq N` | Number of frequencies (1, 2, or 3) |
| `-x`, `--trace LEVEL` | Debug trace level (0-5) |
| `-?`, `--help` | Show help (`-h` is the fix-and-hold AR flag here, **not** help) |

L6E/L6D correction files are passed as additional positional inputs after the navigation file(s).
They are recognised by extension:

| Extension | Content | Used as |
|-----------|---------|---------|
| `*.200.l6`, `*.201.l6` | MADOCA-PPP L6D wide-area ionosphere (PRN 200/201) | ionospheric corrections (any mode) |
| `.l6` | CLAS L6D or MADOCA-PPP L6E | CLAS in CLAS / PPP-RTK modes, otherwise MADOCA-PPP L6E |
| `.l6d` | QZSS L6D | ionosphere if named `*.200.l6d` / `*.201.l6d` or if its first frame is a MADOCA-PPP ionosphere frame; otherwise CLAS (CLAS / PPP-RTK modes only, ignored with a warning in other modes) |
| `.l6e` | QZSS L6E | MADOCA-PPP L6E; in CLAS / PPP-RTK modes reserved for CLAS CMA authentication and ignored with a warning (not supported yet) |

The extensions are case-insensitive (`.L6`, `.L6D`, `.L6E`). L6 files are never
read as RINEX.

**Examples:**

```bash
# MADOCA PPP
mrtk post --config conf/madocalib/rnx2rtkp.toml obs.obs nav.nav l6e.l6

# MADOCA PPP-AR
mrtk post --config conf/madocalib/rnx2rtkp_pppar.toml obs.obs nav.nav l6e.l6

# CLAS PPP-RTK (single channel)
mrtk post --config conf/claslib/rnx2rtkp.toml obs.obs nav.nav clas.l6

# CLAS PPP-RTK (dual channel)
mrtk post --config conf/claslib/rnx2rtkp.toml obs.obs nav.nav \
  clas_ch1.l6 clas_ch2.l6

# CLAS PPP-RTK with claslib-style .l6d input
mrtk post --config conf/claslib/rnx2rtkp.toml obs.bnx 2023044J.l6d
```

---

### mrtk run

Run real-time positioning pipeline (formerly `rtkrcv`).

```bash
mrtk run [options]
```

| Option | Description |
|--------|-------------|
| `-s` | Start processing immediately |
| `-o`, `--config FILE` | Processing options / configuration file (TOML) |
| `-p`, `--port PORT` | Telnet console port |
| `-m PORT` | Monitor stream port |
| `-d`, `--device DEV` | Terminal device for the interactive console |
| `-t LEVEL`, `--trace LEVEL` | Debug trace level |
| `-h`, `--help` | Show help |

**Examples:**

```bash
# CLAS PPP-RTK (single channel, auto-start)
mrtk run -s --config conf/claslib/rtkrcv.toml

# CLAS PPP-RTK (dual channel)
mrtk run -s --config conf/claslib/rtkrcv_2ch.toml

# MADOCA PPP with console
mrtk run -s --config conf/malib/rtkrcv.toml --port 2105
```

---

## Data & Streaming

### mrtk relay

Relay and split data streams (formerly `str2str`).

```bash
mrtk relay [options]
```

| Option | Description |
|--------|-------------|
| `-in`, `--input STREAM[#FORMAT]` | Input stream path and format |
| `-out`, `--output STREAM[#FORMAT]` | Output stream path(s) and format |
| `-msg TYPE` | RTCM message types to relay |
| `-t`, `--trace LEVEL` | Debug trace level |
| `-h`, `--help` | Show help |

**Example:**

```bash
mrtk relay --input ntrip://user:pass@caster:2101/mount \
           --output tcpsvr://:2102
```

---

### mrtk convert

Convert receiver raw data to RINEX (formerly `convbin`).

```bash
mrtk convert [options] rawfile
```

| Option | Description |
|--------|-------------|
| `-o`, `--output FILE` | Output observation file |
| `-n`, `--nav FILE` | Output navigation file |
| `-r FORMAT` | Receiver raw format |
| `-v VER` | RINEX version (2.11, 3.03, 3.04, 4.00) |
| `-ts`, `--start Y/M/D H:M:S` | Start time |
| `-te`, `--end Y/M/D H:M:S` | End time |
| `-ti`, `--interval SEC` | Observation interval |
| `-f`, `--freq N` | Number of frequencies |
| `-trace LEVEL`, `--trace LEVEL` | Trace level |
| `--help` | Show help (`-h` is the HNAV-output flag here, **not** help) |

**Example:**

```bash
mrtk convert -r ubx -v 3.04 --output obs.obs --nav nav.nav raw.ubx
```

**Frequency ordering and `-f` / `--freq N`.** Observation types are written in
the library's obsdef band order — the same order the positioning engines use —
so the *N*-th band selected by `-f N` (`--freq N`) matches what `mrtk post` /
`mrtk run` would use. Per constellation that order is:

| System | band 1 | band 2 | band 3 | band 4 | band 5 |
|--------|--------|--------|--------|--------|--------|
| GPS | L1 | L2 | L5 | – | – |
| GLONASS | G1 | G2 | – | – | – |
| Galileo | E1 | **E5a** | **E5b** | E6 | E5a+b |
| QZSS | L1 | **L5** | **L2** | L6 | – |
| BeiDou | B1I | B3 | B2b | B1C | B2a |
| SBAS | L1 | L5 | – | – | – |
| NavIC | L5 | S | – | – | – |

So `mrtk convert -f 2` keeps the first two bands of each system (e.g. Galileo
E1 + E5a). All bands above are emitted at the default (`-f 5`).

!!! note "Known limitations: GLONASS G3 and BeiDou B2a+b"
    Two rare signals have no slot in the obsdef frequency tables and are not
    emitted to RINEX: **GLONASS G3** (CDMA, GLONASS-K2 only) and **BeiDou
    B2a+b** (AltBOC, beyond the converter's 5-band `--freq` mask). Adding slots
    for them would perturb GLONASS / BeiDou positioning, so the tables are left
    as-is (see [#71](https://github.com/h-shiono/MRTKLIB/issues/71)).

---

## Format Translation

### mrtk ssr2obs

Convert SSR corrections to pseudo-observations.

```bash
mrtk ssr2obs [options] obsfile navfile [navfile...] [l6file...]
```

| Option | Description |
|--------|-------------|
| `-k`, `--config FILE` | Configuration file |
| `-o`, `--output FILE` | Output observation file |
| `-ts`, `--start Y/M/D H:M:S` | Start time |
| `-te`, `--end Y/M/D H:M:S` | End time |
| `-ti`, `--interval SEC` | Interval |
| `-x`, `--trace LEVEL` | Trace level |
| `-h`, `--help` | Show help |

---

### mrtk ssr2osr

Convert SSR corrections to Observation Space Representation (OSR).

```bash
mrtk ssr2osr [options] obsfile navfile [navfile...] [l6file...]
```

| Option | Description |
|--------|-------------|
| `-k`, `--config FILE` | Configuration file |
| `-o`, `--output FILE` | Output OSR file (default: stdout) |
| `-ts`, `--start Y/M/D H:M:S` | Start time |
| `-te`, `--end Y/M/D H:M:S` | End time |
| `-ti`, `--interval SEC` | Interval |
| `-x`, `--trace LEVEL` | Trace level |
| `-h`, `--help` | Show help |

---

### mrtk cssr2rtcm3

Convert CLAS CSSR corrections to RTCM3 MSM messages in real time, enabling CLAS-incompatible receivers to use CLAS as a VRS source. The MSM message type is configurable via TOML (default: MSM7; MSM4 / MSM5 also supported).

```bash
mrtk cssr2rtcm3 [options] [-nav file ...]
```

| Option | Description |
|--------|-------------|
| `-k`, `--config FILE` | Configuration file (TOML or legacy `.conf`) |
| `-in`, `--input URI` | L6 CSSR input stream; use `sbf://…` for single-stream SBF (auto-extracts L6D, PVT, NAV) |
| `-2ch URI` | Second L6 input stream (channel 2) |
| `-out`, `--output URI` | Output RTCM3 stream (e.g. `tcpsvr://:2101`) |
| `-pos URI` | NMEA GGA input stream for rover position (optional) |
| `-p LAT,LON,HGT` | Fixed user position (deg, m) |
| `-nav`, `--nav FILE…` | Navigation file(s) (RINEX NAV) |
| `-d`, `--trace LEVEL` | Trace level (0-5) |
| `-t`, `--interval SEC` | Output interval in seconds (default: 1) |
| `-h`, `--help` | Show help |

**Stream URI formats:**

- Serial: `serial://ttyACM0:115200`
- TCP server: `tcpsvr://:port`
- TCP client: `tcpcli://host:port`
- NTRIP client: `ntripcli://user:pass@host:port/mount`
- File: `file://path`

---

## Utilities

### mrtk bias

Estimate receiver fractional cycle biases (formerly `recvbias`).

```bash
mrtk bias [options] obsfile navfile [l6file...]
```

| Option | Description |
|--------|-------------|
| `-nav`, `--nav FILE` | RINEX NAV file (required) |
| `-out`, `--output FILE` | Output bias file (default: stdout) |
| `-td Y/M/D` | Date (GPST, required) |
| `-ts TSPAN` | Time span in **days** (not a start time) |
| `-t`, `--trace LEVEL` | Trace level |
| `-h`, `--help` | Show help |

> Note: in `mrtk bias` the short flag `-ts` is the time-span value
> (`tspan` in days), not a start-time. There is intentionally no
> `--start` long alias here.

---

### mrtk dump

Dump stream data to human-readable format (formerly `dumpcssr`).

```bash
mrtk dump [options] l6file [navfile...]
```

| Option | Description |
|--------|-------------|
| `-k`, `--config FILE` | Configuration file |
| `-o`, `--output FILE` | Output dump file (default: stdout) |
| `-ts`, `--start Y/M/D H:M:S` | Start time |
| `-te`, `--end Y/M/D H:M:S` | End time |
| `-ch N` | L6 channel (0 or 1) |
| `-x`, `--trace LEVEL` | Trace level |
| `-h`, `--help` | Show help |

---

### mrtk l6extract

Extract QZSS L6 frames from SBF or UBX binary logs into per-PRN `.l6` files.

```bash
mrtk l6extract [options]
```

| Option | Description |
|--------|-------------|
| `-in`, `--input FILE` | Input SBF or UBX binary file (required) |
| `-r FORMAT` | Receiver format: `sbf`, `ubx` (auto-detected from extension by default) |
| `-o`, `--output PREFIX` | Output file prefix (default: `l6`) |
| `-l6d` | Extract L6D frames only (CLAS) |
| `-l6e` | Extract L6E frames only (MADOCA) |
| `-h`, `--help` | Show help |

---

## Global Options

| Option | Description |
|--------|-------------|
| `--help`, `-h` | Show help message (top-level dispatcher) |
| `--version`, `-v` | Show version |

> Inside a subcommand, the help-flag policy is per-subcommand. `-h` is the
> help flag in eight of the ten subcommands; the exceptions are
> [`mrtk post`](#mrtk-post) (where `-h` means fix-and-hold AR) and
> [`mrtk convert`](#mrtk-convert) (where `-h` is the HNAV output file).
> In those two subcommands, use `--help` (or `-?` in `mrtk post`).
