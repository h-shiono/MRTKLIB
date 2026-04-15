---
name: Bug report
about: Report a build failure, crash, or incorrect behavior unrelated to positioning accuracy
title: "[Bug] <short summary>"
labels: ["bug", "status:needs-triage"]
assignees: []
---

<!--
Thanks for reporting a bug. Please fill in each section so we can reproduce
and triage quickly. For positioning-accuracy or Fix-rate regressions,
use the "Positioning issue" template instead.
-->

## Summary

<!-- One or two sentences: what went wrong? -->

## Environment

- MRTKLIB version / commit: <!-- e.g. v0.6.1 or 7cff756 -->
- OS and version: <!-- e.g. macOS 14.5 (arm64), Ubuntu 22.04, Windows 11 -->
- Compiler and version: <!-- e.g. Apple clang 15.0.0, gcc 11.4.0, MSVC 19.38 -->
- CMake version: <!-- `cmake --version` -->
- vcpkg commit (if relevant): <!-- `cd vcpkg && git rev-parse HEAD` -->
- Subcommand involved: <!-- e.g. mrtk run, mrtk post, mrtk relay, build-only -->

## Steps to reproduce

1.
2.
3.

<!--
Include the exact command line, TOML config path, and input data source
(SBF / RTCM3 / RINEX file, stream URL — redact credentials). If the data
cannot be shared, describe it (receiver, duration, constellations).
-->

## Expected behavior

<!-- What should have happened? -->

## Actual behavior

<!-- What actually happened? Include the error message verbatim. -->

## Logs / output

Paste stderr / trace output below. Keep it short; attach full logs as files if large.

<details>
<summary>Relevant log excerpt</summary>

```
# paste log output here
```

</details>

## Additional context

<!-- Anything else: recent changes, related issues, screenshots, etc. -->
