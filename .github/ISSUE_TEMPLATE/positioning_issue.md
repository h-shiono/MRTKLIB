---
name: Positioning accuracy / Fix-rate issue
about: Report unexpected positioning accuracy, Fix-rate degradation, or a regression against a known baseline
title: "[Positioning] <mode> <short summary>"
labels: ["regression", "status:needs-triage"]
assignees: []
---

<!--
Use this template when the code builds and runs, but the positioning
solution is wrong or worse than expected (accuracy, Fix rate, convergence
time, etc.). For crashes or build failures, use the "Bug report" template.
-->

## Summary

<!-- One or two sentences: which mode, what degraded, by how much. -->

## Positioning configuration

- Mode: <!-- SPP / RTK / PPP / PPP-AR / PPP-RTK / VRS-RTK -->
- Processing type: <!-- real-time (mrtk run) or post-processing (mrtk post) -->
- Correction source: <!-- CLAS (L6D) / MADOCA (L6E) / MADOCA-PPP / none -->
- Constellations enabled: <!-- e.g. GPS + Galileo + QZSS -->
- TOML config: <!-- path to config; attach if not upstream; redact secrets -->

## Environment

- MRTKLIB version / commit: <!-- e.g. v0.6.1 or 7cff756 -->
- OS: <!-- e.g. macOS 14.5, Ubuntu 22.04 -->
- Receiver (if relevant): <!-- e.g. Septentrio mosaic-G5, u-blox ZED-F9P -->

## Dataset

- Observation / input file: <!-- SBF / RTCM3 / RINEX; duration; date; location -->
- Correction input: <!-- L6 stream, SBF with QZSL6 blocks, etc. -->
- Is the dataset shareable? <!-- yes / no; if no, describe it -->

## Observed vs. expected

| Metric                | Expected    | Observed |
|-----------------------|-------------|----------|
| Fix rate              |             |          |
| Horizontal accuracy   |             |          |
| Vertical accuracy     |             |          |
| Time to first fix     |             |          |
| Longest continuous Fix|             |          |

## Baseline for comparison

<!--
If this is a regression, what are you comparing against?
e.g. "same dataset on v0.6.0 gave 99.4% Fix; on v0.6.1 gives 67%"
Include commit hashes if known.
-->

## Reproduction

```
<!-- Exact command line(s). -->
```

## Logs / plots

<details>
<summary>Relevant output</summary>

```
<!-- trace excerpt, .pos tail, NMEA snippet, or similar -->
```

</details>

<!-- Attach screenshots of ENU plots or scatter plots if available. -->

## Additional context

<!-- Anything else: related issues, upstream behavior, hypotheses. -->
