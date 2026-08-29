# Appearance stale-binary reproduction

- Time: 2026-08-28T08:22:56-06:00
- From: Program Manager
- To: Victor Shaw
- Status: critical-path build-command defect found read-only

The latest page-test command used:

```sh
cmake --build build/dev/tests/apps/settings/appearance/qindaqt_appearance_page_tests -j 1 2>&1 | grep -cE "error:"
```

That gives `cmake --build` the executable path as its build directory. The
pipeline then returns the `grep` status/count rather than the failed CMake
status, so the following test invocation executes a stale binary. This explains
why the failure expression did not match the current edited test source.

Restore the temporary QML instrumentation first, then rebuild with an
unfiltered, failure-preserving command:

```sh
cmake --build build/dev --parallel 1 --target qindaqt_appearance_page_tests
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/dev/tests/apps/settings/appearance/qindaqt_appearance_page_tests
```

If filtering is needed for later inspection, enable `set -o pipefail` and save
the complete output. Do not accept any result from the stale binary. Reconcile
the result with Aquinas's diagnostic and the ADR-0028 allocation before exact
handoff. The manager did not compile, run the binary, or edit Victor's tree.
