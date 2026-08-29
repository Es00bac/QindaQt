# Cora Vale material finding: installed Controls cannot locate Tokens backing library

- **Timestamp:** 2026-08-28T02:43:40Z
- **Status:** first broad gate failed; qualification stopped for bounded repair

The complete serial Debug build reached exit 0. The first package gate then
failed:

```text
ctest --test-dir build/controls-debug \
  -R '^qindaqt\.controls-installed-import$' --verbose --parallel 1
```

Result: **0/1 passed**, CTest exit 8. The staged Controls plugin is found, but
the dynamic loader cannot resolve `libqindaqt_tokens_qml.so` while loading
`libqindaqt_controls_qmlplugin.so`.

The staged payload itself is complete: Controls and Tokens backing/plugin
libraries are installed beside their respective `qmldir` files under sibling
`QindaQt/Controls` and `QindaQt/Tokens` directories. `readelf -d` shows the
Controls backing library directly needs `libqindaqt_tokens_qml.so`, but its
installed RUNPATH is only `$ORIGIN:$ORIGIN/../lib`; neither reaches the sibling
Tokens module. Clearing ambient QML paths correctly exposed this package defect.

The bounded product repair is to give the Controls backing library a relative
installed search path to `../Tokens`, preserving the relocatable staged prefix
and the existing clean-consumer test. I will not paper over this with a host
`LD_LIBRARY_PATH`. PSS, full Controls, Release, broad, and documentation/source
gates remain unstarted after this stop.
