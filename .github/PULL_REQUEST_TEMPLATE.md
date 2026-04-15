<!--
Thanks for contributing to MRTKLIB. Please fill in the sections below.
Delete any section that does not apply.
-->

## Summary

<!-- What does this PR change, and why? One short paragraph. -->

## Related issues

<!-- e.g. Closes #123, Refs #456 -->

## Type of change

- [ ] Bug fix (non-breaking)
- [ ] New feature (non-breaking)
- [ ] Refactor (no functional change)
- [ ] Documentation only
- [ ] Build / CI / tooling
- [ ] Breaking change (describe migration below)

## Testing

<!-- How did you verify this change? Paste the `ctest` summary below. -->

```
$ cd build && ctest --output-on-failure
<!-- paste the summary line, e.g. "62/62 tests passed" -->
```

- [ ] Existing test suite still passes
- [ ] New tests added for new behavior (or explain why not)

## Positioning regression check

<!--
For changes that touch the positioning pipeline (PPP, RTK, PPP-RTK, VRS-RTK,
SPP, CLAS, MADOCA, RTCM decoding, etc.), confirm that Fix rate and accuracy
on the relevant reference dataset did not regress. State the dataset and
the observed numbers.
-->

- [ ] Not applicable (change does not affect the positioning pipeline)
- [ ] Regression check performed; dataset and results:

## Breaking changes / migration notes

<!-- TOML keys renamed, CLI flags removed, public API changed, etc. -->

## Checklist

- [ ] I have read the coding standards in `CLAUDE.md`
- [ ] Doxygen comments added/updated for new or changed public functions
- [ ] `.clang-format` applied (or diff is formatting-clean)
- [ ] No unrelated changes included in this PR
