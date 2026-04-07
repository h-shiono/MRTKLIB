# NTRIP Streams

This guide covers how to connect MRTKLIB to NTRIP casters for real-time
GNSS correction data, including NTRIP version selection and TLS (encrypted)
connections.

---

## NTRIP Path Format

All NTRIP stream paths follow this format:

```
[user[:password]@]host[:port]/mountpoint[?ver=N][&host=HOSTNAME]
```

| Component | Description | Default |
|:----------|:------------|:--------|
| `user` | Username for authentication | _(none)_ |
| `password` | Password (special chars: use `%XX` encoding) | _(none)_ |
| `host` | Caster hostname or IP | _(required)_ |
| `port` | TCP port | 2101 (client), 80 (server) |
| `mountpoint` | Stream mountpoint name | _(required)_ |
| `?ver=N` | NTRIP version override (1 or 2) | auto |
| `&host=HOSTNAME` | Host header override (for TLS tunnels) | _(from addr:port)_ |

### Special Characters in Passwords

Characters that conflict with the path syntax must be percent-encoded:

| Character | Encoding |
|:----------|:---------|
| `@` | `%40` |
| `:` | `%3A` |
| `/` | `%2F` |
| `%` | `%25` |

MRTKLIB automatically decodes these before authentication.

**Example:** password `p@ss:word` becomes `p%40ss%3Aword` in the path.

---

## NTRIP Versions

MRTKLIB supports both NTRIP v1 and v2.

| Feature | v1 | v2 |
|:--------|:---|:---|
| Protocol | ICY / HTTP/1.0 | HTTP/1.1 |
| Server request | `SOURCE` command | `POST` with chunked encoding |
| Transfer encoding | Raw stream | Chunked (RFC 7230) |
| Typical port | 2101 | 2101 |

### Version Selection

By default, MRTKLIB uses **auto-detection**: it sends a v2 request first
and falls back to v1 if the caster responds with an ICY (v1) header or
rejects the connection.

To force a specific version, append `?ver=N` to the mountpoint:

=== "Auto (default)"

    ```toml
    [streams.input.correction]
    type = "ntripcli"
    path = "user:passwd@caster.example.com:2101/RTCM3_MOUNT"
    ```

=== "Force v1"

    ```toml
    [streams.input.correction]
    type = "ntripcli"
    path = "user:passwd@caster.example.com:2101/RTCM3_MOUNT?ver=1"
    ```

=== "Force v2"

    ```toml
    [streams.input.correction]
    type = "ntripcli"
    path = "user:passwd@caster.example.com:2101/RTCM3_MOUNT?ver=2"
    ```

CLI usage with `mrtk relay`:

```bash
mrtk relay -in ntrip://user:passwd@caster.example.com:2101/MOUNT?ver=2 \
           -out file://output.rtcm3
```

---

## TLS Connections (NTRIP 2s)

Some casters require TLS-encrypted connections on port 443 (often called
"NTRIP 2s"). Examples include NASA Earthdata CDDIS.

MRTKLIB does not currently include a built-in TLS stack. Use **stunnel**
to create a local TLS tunnel.

### Setup with stunnel

#### 1. Install stunnel

=== "macOS"

    ```bash
    brew install stunnel
    ```

=== "Ubuntu / Debian"

    ```bash
    sudo apt install stunnel4
    ```

#### 2. Create a configuration file

Create `ntrip-tls.conf`:

```ini
[ntrip-tls]
client = yes
accept = 127.0.0.1:2101
connect = caster.cddis.eosdis.nasa.gov:443
```

This listens on local port 2101 and forwards traffic over TLS to the
remote caster on port 443.

#### 3. Start the tunnel

```bash
stunnel ntrip-tls.conf
```

#### 4. Connect MRTKLIB through the tunnel

Point MRTKLIB at the local stunnel endpoint. Use `&host=` to set the
correct Host header for the remote caster:

```toml
[streams.input.correction]
type = "ntripcli"
path = "user:p%40ssword@127.0.0.1:2101/MOUNT?ver=2&host=caster.cddis.eosdis.nasa.gov"
```

Or via CLI:

```bash
mrtk relay -in "ntrip://user:p%40ssword@127.0.0.1:2101/MOUNT?ver=2&host=caster.cddis.eosdis.nasa.gov" \
           -out file://output.rtcm3
```

!!! warning "Host header is required for most TLS casters"
    When connecting through stunnel, the NTRIP v2 `Host:` header defaults
    to `localhost:2101`. Many casters use this header for routing and will
    return 404 if it doesn't match. Always add `&host=<real-hostname>` when
    using a TLS tunnel with NTRIP v2.

!!! note "NASA Earthdata Credentials"
    NASA Earthdata uses your Earthdata Login credentials.
    Register at [https://urs.earthdata.nasa.gov/](https://urs.earthdata.nasa.gov/).
    Passwords often contain special characters --- remember to percent-encode
    `@` as `%40` and `:` as `%3A` in the path.

### Alternative: socat (one-liner)

For quick testing without a configuration file:

```bash
socat TCP-LISTEN:2101,fork,reuseaddr \
      OPENSSL:caster.cddis.eosdis.nasa.gov:443
```

Then connect MRTKLIB to `127.0.0.1:2101` as above.

---

## Troubleshooting

### Authentication failure (401)

- Verify credentials are correct.
- Check that `@` and `:` in the password are percent-encoded (`%40`, `%3A`).
- Some casters require NTRIP v2 --- try `?ver=2`.

### Connection refused or timeout

- Confirm the caster hostname and port are reachable:
  ```bash
  nc -zv caster.example.com 2101
  ```
- For TLS casters (port 443), verify the stunnel tunnel is running:
  ```bash
  nc -zv 127.0.0.1 2101
  ```

### Source table received instead of data

- The mountpoint name is incorrect or does not exist on the caster.
- Request the source table to list available mountpoints:
  ```bash
  mrtk relay -in ntrip://user:passwd@caster.example.com:2101/ \
             -out file://sourcetable.txt
  ```

### v2 handshake fails, falls back to v1

- This is normal for v1-only casters. No action needed.
- Check `mrtk run` status output: `ver_neg = 1` confirms v1 negotiation.
- To suppress the fallback attempt, use `?ver=1`.
