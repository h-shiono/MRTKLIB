# Release Notes — v0.6.3

## NTRIP v2 Protocol Support

**Release date:** 2026-03-31
**Type:** Feature — NTRIP v2 (HTTP/1.1) protocol support with auto-negotiation
**Branch:** `feat/ntrip-v2`

---

### Overview

MRTKLIB now supports the NTRIP v2 protocol (RTCM 10410.1) alongside the
existing v1 implementation. NTRIP v2 uses standard HTTP/1.1 with chunked
transfer encoding, replacing the non-standard ICY/HTTP/1.0 protocol of v1.

By default, new connections use **auto-detection**: the client sends a v2
request first and transparently falls back to v1 if the caster does not
support it. Per-stream version override is available via the `?ver=N`
query parameter.

---

### What's New

#### NTRIP v2 Client

- HTTP/1.1 GET requests with `Host:` and `Ntrip-Version: Ntrip/2.0` headers
- Automatic parsing of `HTTP/1.1 200 OK` responses (v2) alongside `ICY 200 OK` (v1)
- Chunked transfer decoding for incoming data streams
- Version auto-detection with seamless v1 fallback

#### NTRIP v2 Server

- HTTP/1.1 POST requests replace the legacy `SOURCE` command
- Chunked transfer encoding for outbound data streams

#### NTRIP v2 Caster

- Extended existing v2 response generation to include chunked data delivery
- Per-client version tracking (v1 clients receive raw data, v2 clients receive chunked)

#### Chunked Transfer Encoding

- Header-only codec (`ntrip_chunk.h`) with incremental, non-blocking state machine decoder
- Stateless encoder for outbound chunk framing
- HTTP header helpers for status code extraction and header lookup

#### URL Percent-Encoding

- `decodetcppath()` now decodes `%XX` sequences in user and password fields
- Enables passwords containing `@` (`%40`), `:` (`%3A`), and other special characters

#### Version Selection

- **Auto (default):** Try v2, fall back to v1 on failure or ICY response
- **`?ver=1`:** Force NTRIP v1 (original behavior)
- **`?ver=2`:** Force NTRIP v2 (HTTP/1.1)
- `strsetntripver()` API for setting the global default version

#### Documentation

- New **NTRIP Streams** guide (`docs/guide/ntrip.md`) covering path format,
  version selection, TLS tunnel setup with stunnel/socat, and troubleshooting

---

### Files Added

| File | Description |
|------|-------------|
| `src/stream/ntrip_chunk.h` | Header-only chunked transfer codec and HTTP helpers |
| `tests/utest/t_ntrip.c` | 17 unit tests for chunked codec and HTTP helpers |
| `docs/guide/ntrip.md` | NTRIP streams user guide |

### Files Changed

| File | Change |
|------|--------|
| `src/stream/mrtk_stream.c` | v2 protocol support: struct extensions, request/response handlers, chunked data path, URL decoding, version negotiation |
| `include/mrtklib/mrtk_stream.h` | `strsetntripver()` declaration |
| `CMakeLists.txt` | Version 0.6.2 -> 0.6.3; `t_ntrip` test target |
| `CHANGELOG.md` | v0.6.3 entry |
| `README.md` | v0.6.3 in roadmap |
| `mkdocs.yml` | NTRIP Streams guide in navigation |

---

### Test Results

63/63 tests pass (62 existing + 1 new `utest_t_ntrip`; no regressions).
