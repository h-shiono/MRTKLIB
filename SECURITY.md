# Security Policy

## Reporting a vulnerability

If you believe you have found a security vulnerability in MRTKLIB, please
**do not open a public GitHub issue**.

Report it privately using GitHub's **Private Vulnerability Reporting**:

1. Go to <https://github.com/h-shiono/MRTKLIB/security/advisories/new>
2. Describe the issue, including:
   - Affected component(s) (e.g. `mrtk run`, RTCM3 decoder, TOML parser)
   - Affected version or commit hash
   - Steps to reproduce, ideally with a minimal dataset or command line
   - The impact you observed or can demonstrate
3. Submit the advisory. Only the maintainers and invited collaborators
   will be able to see it.

You will receive an acknowledgement within a reasonable time frame.
Because MRTKLIB is maintained by a small team, please be patient if the
first response takes a few days.

## What we consider in scope

- Crashes, memory-safety issues (out-of-bounds reads/writes,
  use-after-free), or undefined behavior triggered by crafted inputs:
  RTCM3 streams, BINEX messages, UBX frames, SBF blocks, L6D/L6E
  correction payloads, RINEX files, TOML configuration files
- Path traversal or arbitrary-file-write issues in file-handling paths
- Network-protocol weaknesses in the stream layer (NTRIP client/server,
  TCP/UDP handlers) that could be exploited by a malicious peer
- Authentication or credential-handling bugs in the stream layer

## What we do not consider security issues

- Incorrect positioning results or Fix-rate regressions — please use the
  "Positioning accuracy / Fix-rate issue" template under
  <https://github.com/h-shiono/MRTKLIB/issues/new/choose>
- Bugs that require local root access to exploit
- Denial of service through abnormally large input files, absent a
  concrete exploitation path

## Coordinated disclosure

We follow a coordinated-disclosure model. After triage we will work with
you on a fix timeline; public disclosure happens once a patch is
available or after a mutually agreed deadline. You will be credited in
the advisory unless you prefer to remain anonymous.
