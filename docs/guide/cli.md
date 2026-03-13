# CLI Reference

MRTKLIB provides a single unified binary `mrtk` with subcommands (BusyBox/Git pattern).

```
mrtk [COMMAND] [OPTIONS]
```

## Core Commands

### mrtk post

Run post-processing positioning (formerly `rnx2rtkp`).

```bash
mrtk post [options] obsfile navfile [navfile...] [l6file...]
```

| Option | Description |
|--------|-------------|
| `-k file` | Configuration file (TOML) |
| `-o file` | Output position file |
| `-ts y/m/d h:m:s` | Start time |
| `-te y/m/d h:m:s` | End time |
| `-ti tint` | Processing interval (seconds) |
| `-p mode` | Positioning mode (0:single, 1:dgps, ..., 9:ppp-rtk) |
| `-f freq` | Number of frequencies (1, 2, or 3) |
| `-x level` | Debug trace level (0-5) |

L6E/L6D correction files are passed as additional positional inputs after the navigation file(s).

**Examples:**

```bash
# MADOCA PPP
mrtk post -k conf/madocalib/rnx2rtkp.toml obs.obs nav.nav l6e.l6

# MADOCA PPP-AR
mrtk post -k conf/madocalib/rnx2rtkp_pppar.toml obs.obs nav.nav l6e.l6

# CLAS PPP-RTK (single channel)
mrtk post -k conf/claslib/rnx2rtkp.toml obs.obs nav.nav clas.l6

# CLAS PPP-RTK (dual channel)
mrtk post -k conf/claslib/rnx2rtkp.toml obs.obs nav.nav \
  clas_ch1.l6 clas_ch2.l6
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
| `-o file` | Configuration file (TOML) |
| `-p port` | Telnet console port |
| `-m port` | Monitor stream port |
| `-t level` | Debug trace level |

**Examples:**

```bash
# CLAS PPP-RTK (single channel, auto-start)
mrtk run -s -o conf/claslib/rtkrcv.toml

# CLAS PPP-RTK (dual channel)
mrtk run -s -o conf/claslib/rtkrcv_2ch.toml

# MADOCA PPP with console
mrtk run -s -o conf/malib/rtkrcv.toml -p 2105
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
| `-in stream` | Input stream path |
| `-out stream` | Output stream path(s) |
| `-msg type` | RTCM message types to relay |

**Example:**

```bash
mrtk relay -in ntrip://user:pass@caster:2101/mount \
           -out tcpsvr://:2102
```

---

### mrtk convert

Convert receiver raw data to RINEX (formerly `convbin`).

```bash
mrtk convert [options] rawfile
```

| Option | Description |
|--------|-------------|
| `-o file` | Output observation file |
| `-n file` | Output navigation file |
| `-r format` | Receiver raw format |
| `-v ver` | RINEX version (2.11, 3.03, 3.04, 4.00) |

**Example:**

```bash
mrtk convert -r ubx -v 3.04 -o obs.obs -n nav.nav raw.ubx
```

---

## Format Translation

### mrtk ssr2obs

Convert SSR corrections to pseudo-observations.

```bash
mrtk ssr2obs [options] obsfile navfile [navfile...] [l6file...]
```

| Option | Description |
|--------|-------------|
| `-k file` | Configuration file |
| `-o file` | Output observation file |

---

### mrtk ssr2osr

Convert SSR corrections to Observation Space Representation (OSR).

```bash
mrtk ssr2osr [options] obsfile navfile [navfile...] [l6file...]
```

---

## Utilities

### mrtk bias

Estimate receiver fractional cycle biases (formerly `recvbias`).

```bash
mrtk bias [options] obsfile navfile [l6file...]
```

| Option | Description |
|--------|-------------|
| `-k file` | Configuration file |
| `-o file` | Output bias file |

---

### mrtk dump

Dump stream data to human-readable format (formerly `dumpcssr`).

```bash
mrtk dump [options] l6file [navfile...]
```

| Option | Description |
|--------|-------------|
| `-o file` | Output dump file |
| `-ts y/m/d h:m:s` | Start time |
| `-te y/m/d h:m:s` | End time |

---

## Global Options

| Option | Description |
|--------|-------------|
| `--help`, `-h` | Show help message |
| `--version`, `-v` | Show version |
