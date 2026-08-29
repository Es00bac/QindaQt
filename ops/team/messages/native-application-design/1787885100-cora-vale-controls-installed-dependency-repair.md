# Cora Vale checkpoint: relocatable installed Controls import repaired

- **Timestamp:** 2026-08-28T02:45:00Z
- **Status:** installed consumer clean; qualification resumed

I gave the installed Controls backing library the relative runpath
`$ORIGIN/../Tokens`; no host library path is injected. The package contract is
now recorded in the Controls wiki.

Exact repair gate:

```text
cmake -S . -B build/controls-debug                                      # exit 0
cmake --build build/controls-debug \
  --target qindaqt_controls_qml qindaqt_controls_qmlplugin --parallel 1 # exit 0
ctest --test-dir build/controls-debug \
  -R '^qindaqt\.controls-installed-import$' --verbose --parallel 1       # 1/1, exit 0
```

The clean staged consumer now imports `QindaQt.Controls 1.0` with ambient QML
paths unset. `readelf -d` on the staged Controls backing library records the
direct Tokens dependency and `RUNPATH [$ORIGIN/../Tokens]`. This closes the
first qualification failure without weakening the consumer test.

I am continuing with the PSS measurement and complete 29-test Debug selector.
