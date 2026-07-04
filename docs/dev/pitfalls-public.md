# MRTKLIB — Developer Pitfalls

A curated reference of non-obvious behaviors that recur when developing on
MRTKLIB. Each entry describes the trap, the rule, and a remediation pattern.
These are derived from real maintenance experience and are useful both for
new contributors and for AI coding assistants operating on the codebase.

The entries are deliberately abstract — they teach a principle rather than
narrating a specific incident. The maintainer keeps a richer internal log of
project-specific investigations; what appears here is the distilled rule set.

---

## Build & Toolchain

### P-01 — Global `-ansi` flag forces C90 on every new target

`CMakeLists.txt` applies `add_compile_options(-ansi -pedantic)` repository-wide.
Only a small number of explicitly-configured targets override this with
`-std=c11` (currently the `mrtklib` library and the `t_ntrip` test). New
executables, tests, or examples inherit `-ansi` (= C90) unless the override
is added at the new target.

`set_target_properties(... C_STANDARD 11)` and `target_compile_features(... c_std_11)` do **not** override `-ansi` once it has been pushed onto the command line by `add_compile_options()`; an explicit `target_compile_options(<target> PRIVATE -std=c11)` is required.

A C99/C11 source file (mixed declarations, `for (int i = ...)`, designated
initialisers, `// ...` comments, etc.) compiled under `-ansi` will fail on
strict toolchains even if it builds on macOS by accident.

```cmake
# When adding a new target that uses C99+ syntax:
target_compile_options(<target> PRIVATE -std=c11)
```

### P-02 — `BLA_SIZEOF_INTEGER` is a CMake hint, not an ABI guarantee

CMake's `FindBLAS.cmake` uses `BLA_SIZEOF_INTEGER` only to choose between
library-name candidates (e.g. `openblas` vs `openblas64` / `openblas_64`). It
does **not** read symbol metadata from the resolved library. On platforms
where an ILP64 build of BLAS is packaged under the same filename as the
LP64 build, setting `BLA_SIZEOF_INTEGER=4` will still resolve to ILP64 silently
and crash at first call.

MRTKLIB sets `BLA_SIZEOF_INTEGER=4` (LP64) on CMake ≥ 3.22 and rejects an
explicit `=8` override with `FATAL_ERROR`. Treat this as a *request* in
documentation and code comments, not as enforcement. Wording like
"requests LP64" is accurate; "enforces LP64" is misleading.

If a user reports a crash with `BLA_SIZEOF_INTEGER=4` set, the remediation is
to pass an explicit LP64 provider via `CMAKE_PREFIX_PATH` or `BLAS_LIBRARIES`.

**Reproducible numerics (`-DMRTK_DETERMINISTIC_BLAS=ON`).** The default build
links whatever LAPACK CMake finds (Accelerate on macOS, system LAPACK on
Linux). Threaded BLAS reductions accumulate in a non-deterministic order, so
results vary at the last ULP run to run; on storm-day PPP-RTK that ULP noise
can cross the AR ratio threshold and flip fix/float decisions, making
benchmarks irreproducible. For stable benchmarking/regression, configure with
`-DMRTK_DETERMINISTIC_BLAS=ON` (needs OpenBLAS, e.g. `brew install openblas`,
plus `-DCMAKE_PREFIX_PATH=<openblas-prefix>` if not auto-found). It pins the
provider to OpenBLAS and forces a single thread at load time, so every run is
bit-identical with no `OPENBLAS_NUM_THREADS` env var required. It is slower
than the threaded default and does **not** change the algorithm — only
reduction determinism — so reference tolerances may need re-baselining when
switching providers.

### P-03 — `trace()` is compiled out unless `-DTRACE` is defined

The build does not define `TRACE`, so `trace(level, ...)` calls are no-op
macros. Adding `trace(NULL, 2, "...")` for debugging will produce no output.

Use `fprintf(stderr, ...)` for debug print. Always remove the lines before
committing — leftover stderr prints are noisy in CI logs and may be flagged
by review.

---

## Memory & Allocation

### P-10 — Large structs must be heap-allocated

Several core types are too large for the stack or for static arrays:

| Type | Approximate size |
|------|-----------------|
| `rtksvr_t` | ~972 MB |
| `rtcm_t` | ~103 MB |
| `osb_t` | ~700 KB |
| `clas_corr_t` | ~352 KB |

Stack-allocating `rtcm_t var;` blows the default thread stack on Linux/macOS;
embedding it in `static rtcm_t arr[N];` balloons the binary's BSS into the
gigabyte range and can prevent the executable from loading.

Always allocate via `calloc()` (or `new` in C++) and pass by pointer. When
managing multiple instances, use an array of pointers, not an array of structs.

### P-11 — `MAXSAT` and `MAXOBS` are different limits

`MAXSAT` (~100+ when all constellations are enabled) and `MAXOBS` (96) are
distinct constants. Iterating over satellites with `MAXSAT` while writing
into a `MAXOBS`-sized buffer is a heap-overflow pattern that may surface only
after hours of operation, once enough constellations have accumulated
broadcast ephemeris to push the satellite count past 96.

When the destination buffer is `obsd_t[MAXOBS]`, bound the write index
against the buffer length, not the satellite count:

```c
for (sat = 1; sat <= MAXSAT && n < MAXOBS; sat++) {
    /* ... append to obs[n] ... */
}
```

### P-12 — `glorbit()` has no convergence timeout

GLONASS satellite position from broadcast ephemeris is computed via a
Runge-Kutta integrator (`glorbit()`). The integrator does not check for
non-convergent input — given a corrupted GLONASS ephemeris with extreme
values, it can spin indefinitely (process state `R`, ~30% CPU, no output,
no crash).

When iterating satellites for `satpos()` / `ephpos()` calls, gate the loop
against the constellation allow-list you actually use. For example,
CLAS-based code paths process only GPS / GAL / QZS; passing a stale or
corrupted GLONASS satellite through `ephpos()` is unnecessary and risky.

---

## GNSS Algorithm Pitfalls

### P-20 — Never hardcode frequency slot indices

The obsdef array can be reordered. For example, `apply_pppsig()` reshuffles
obsdef for MADOCA-style signal selection. Code such as:

```c
if (f == 1 && sys == SYS_GAL) continue;   /* WRONG */
```

makes a hidden assumption that slot 1 corresponds to L2. After reordering,
slot 1 may be L5 (E5a) for GAL or QZS — the skip will exclude the wrong band.

Look up the physical band dynamically:

```c
int band = code2freq_num(obs->code[f]);     /* returns 1, 2, 5, … (system-agnostic) */
if (band == 2) continue;                    /* skip L2 band, whatever slot it occupies */
```

The signature is `int code2freq_num(uint8_t code)` — a single code argument; no constellation needed.

Under the default obsdef order for QZS, slot 0 is L1, **slot 1 is L5** (not L2),
and slot 2 is L2. Do not rely on any specific slot mapping without a runtime
lookup.

### P-21 — `nav->eph[]` is time-sorted, not satellite-indexed

`nav->eph[]` is a flat array indexed by insertion order, **not** by satellite
ID. Direct indexing like:

```c
eph_t *eph = &nav->eph[sat - 1];   /* WRONG */
```

returns whatever ephemeris happens to occupy slot `sat-1`, almost never the
correct one for satellite `sat`. The symptom is silent IODE mismatch, which
typically manifests as DD ambiguity loss at every broadcast-ephemeris update
boundary (~30 min for GPS, ~10 min for GAL).

Always go through `seleph()`:

```c
eph_t *eph = seleph(time, sat, iode, nav);
if (!eph) return 0;     /* design for NULL return */
```

Pass `iode = -1` to match any IODE; otherwise the call returns NULL when the
requested IODE is not in the array. With SSR corrections, the requested IODE
may legitimately be missing — the broadcast ephemeris stream and the SSR
stream are not synchronised. Code that calls `seleph()` with a specific IODE
must handle NULL gracefully (skip the satellite, fall back, or buffer a
previous ephemeris generation).

### P-22 — GAL F/NAV ephemeris is in `eph[sat-1+MAXSAT]`, not `eph[sat-1]`

GAL has two ephemeris channels in RTCM3:

| RTCM3 type | Channel | Slot in `nav->eph[]` |
|-----------|---------|---------------------|
| 1046 | I/NAV (E1/E5b) | `eph[sat-1]` |
| 1045 | F/NAV (E5a) | `eph[sat-1+MAXSAT]` |

`encode_type1045()` reads from the F/NAV slot. Copying GAL broadcast
ephemeris into `eph[sat-1]` only is sufficient for the I/NAV-based encoder
(1046) but produces empty 1045 payloads. To publish both, populate both
slots.

### P-23 — MSM signal-ID tables differ per constellation

RTCM 3.3 defines different signal-ID-to-code mappings for each GNSS
(table 3.5-91 for GPS, 3.5-99 for GAL, etc.). Signal ID 8 means `5I` in GPS
but `6C` in GAL. ID 10 means `5X` in GPS but `6B` in GAL.

Reusing a GPS lookup table for GAL produces MSM cells with valid headers but
mis-coded signal identifiers. Receivers either drop the satellite or treat
the signal as a different band, producing meter-level pseudorange errors.

The correct per-constellation tables live in `src/rtcm/mrtk_rtcm3.c`
(`msm_sig_gps[]`, `msm_sig_gal[]`, etc.).

### P-24 — MSM encoder treats `code[j] != 0` as "satellite present"

The MSM7 encoder (`encode_msm_head` / `gen_msm_index` in
`src/rtcm/mrtk_rtcm3e.c`) builds the satellite/signal masks from
`obs[i].code[j] != CODE_NONE`, not from the pseudorange value. An entry with
`code[j]` set but `P[j] = 0.0` is encoded as a valid MSM cell, where the
rough-range field overflows to its 255 sentinel. Some receivers attempt to
use this as real data and produce kilometre-scale jumps in solution output.

Drop the slot cleanly:

```c
/* either drop just this signal: */
obs[ko].code[j] = CODE_NONE;
obs[ko].P[j]    = 0.0;
/* or drop the whole satellite: */
obs[ko].sat = 0;
```

Half-populated cells (`code` set, `P` zero) must never reach the encoder.

### P-25 — Forward declarations require tagged structs

If a header originally defines a type via an anonymous struct:

```c
typedef struct { ... } nav_t;       /* anonymous struct */
```

then other headers cannot forward-declare `nav_t`, and pulling the full
definition into every consumer creates an include cycle. Tag the struct:

```c
struct nav_s { ... };
typedef struct nav_s nav_t;
```

Then a downstream header can forward-declare:

```c
struct nav_s;
typedef struct nav_s nav_t;
```

This is the standard pattern when exposing previously-static helpers across
translation-unit boundaries.

### P-26 — CLAS configurations should use `nf=2`

CLAS broadcasts ambiguity bias for L1+L5 only. Setting `nf=3` requests
ambiguity resolution on a third band (E5b for GAL) where the bias
correction is not provided, which silently degrades the fix rate. Use
`nf=3` only after confirming that an E5b bias source is available (for
example, from a third-party SSR provider running alongside CLAS).

---

## Platform & Runtime

### P-30 — macOS serial: `cu.*` for outbound, not `tty.*`

On macOS, `/dev/tty.*` device nodes block on the DCD (data carrier detect)
line for outbound connections; the `/dev/cu.*` ("call-up") variants do not.
Opening `/dev/tty.usbmodem*` for output to a GNSS receiver or USB serial
bridge will hang at `open()` waiting for DCD, which most receivers never
assert.

For any programmatic outbound use on macOS, always use the `cu.*` form:

```
/dev/cu.usbmodem<NNN>     # good
/dev/tty.usbmodem<NNN>    # blocks on DCD
```

### P-31 — Diagnose a stuck daemon before killing it

When a long-running daemon stops emitting output, capture state before
restarting. A `kill` discards the most useful evidence.

| Observation | Interpretation |
|-------------|---------------|
| Process absent from `ps` | Truly dead. Check core file, `dmesg`, `journalctl` for SIGABRT / OOM |
| `STAT=Z` (zombie) | Parent has not reaped. Child already exited |
| `STAT=D` | Uninterruptible I/O wait — kernel driver or hardware issue |
| **`STAT=R` with high CPU** | **User-space infinite loop. `gdb -p PID` will name it** |
| `STAT=S` with low CPU | Sleeping on a syscall. Check `wchan` |

Useful one-shot commands:

```bash
ps -o pid,etime,rss,vsz,pcpu,stat,cmd -p $(pgrep -f mrtk)
sudo gdb -batch -ex "thread apply all bt" -p PID
sudo cat /proc/PID/wchan
sudo cat /proc/PID/stack
sudo ls -la /proc/PID/fd/
sudo strace -p PID -e trace=read,write,select,poll -c -t
```

A single `gdb -p` snapshot of a `STAT=R` process usually identifies the
loop. Even non-reproducible hangs can often be root-caused from one good
backtrace.

---

## CSSR / IS-QZSS-L6 Reference

### P-40 — CSSR subtype quick reference

| Subtype | Name | Contents |
|---------|------|----------|
| ST1 | Mask | Satellite / signal / cell masks per GNSS |
| ST2 | Orbit | δ radial / along / cross + IODE |
| ST3 | Clock | δ clock C0 |
| ST4 | Code Bias | Per-signal code bias |
| ST5 | Phase Bias | Per-signal phase bias + discontinuity |
| **ST6** | Code + Phase Bias (combined) | Bias only — **no troposphere** |
| ST7 | URA | Per-satellite URA |
| ST8 | STEC | Per-satellite ionosphere polynomial (regional) |
| ST9 | Gridded (legacy) | Per-grid trop + STEC residual, older format |
| ST10 | Service Info | Grid definitions etc. |
| ST11 | Combined Orbit/Clock | Optional network-scoped orbit+clock |
| **ST12** | Atmospheric (STEC + Trop) | `trop_avail` / `stec_avail` flags in header |

Current CLAS deployments carry troposphere corrections in **ST12** (not ST9).
The `trop_avail` field (2 bits) governs whether the hydrostatic and wet
parts are present; `trop_avail == 0` means the message is STEC-only for that
network/epoch. Do not assume every ST12 message updates troposphere.

Reference: IS-QZSS-L6-008 §4.1.2.2.

---

## How to extend this document

Promote a finding from internal notes only when **all** of the following hold:

- The rule generalises to any contributor working on a fresh clone
- The rule is reproducible from the public source code alone
- The wording is self-contained — no backreference to internal incident
  history is needed for a reader to apply the rule
- The phrasing is third-person principle, not a narrated investigation

If a finding requires "we discovered through ... that ..." or names a
specific commit / session / vendor to make sense, it belongs in the
maintainer's local log, not here.

When in doubt, leave it out. Missing one principle here costs an external
contributor a question on the issue tracker; over-sharing internal context
is irreversible.
