# Clipboard C2 denial-control repair handoff

- Candidate: `fef92227bb2edd847d1c88373855d90aca0f042d`; tree: `dae0482a4155b610ec09d405414089ba8b9bef76`.
- Exact rejected base: `a069842659e3e96dd93d0f27a66049d9d3ff03c8`; candidate parent: prior coordination-only handoff `98ecfc53538d4c0b074044399fdcce11684395bb`.
- Changed paths (sorted):
  - `docs/wiki/development/testing-harness.md`
  - `docs/wiki/shell/clipboard-applet.md`
  - `tests/shell/clipboard_applet/CMakeLists.txt`
  - `tests/shell/clipboard_applet/tst_clipboard_applet_composition_private_bus.cpp`
- Evidence, all from this worktree:
  - Exact prescribed Debug and Release CMake configure commands: exit 0 each.
  - `cmake --build <debug|release> --parallel 3 --target qindaqt_clipboard_applet_composition_private_bus_tests`: exit 0 each.
  - `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <debug|release> -R '^qindaqt\.(clipboard-applet-|applet|shell-runtime-)' --output-on-failure --no-tests=error`: exit 0, 29/29 each.
  - Negative control: transiently disabled the production consent-withholding branch, rebuilt Debug, and ran `ctest -R '^qindaqt\.clipboard-applet-consent-'`; exit 8 with 0/4 passing and all four exact-reason assertions failing. Restored the source, rebuilt, and confirmed it matches the committed production file.
  - `./tools/validate-docs`: exit 0, 135 documents plus navigation.
  - strict MkDocs to the assigned build root: exit 0.
  - `./tools/check-source-shape`: exit 0, 2,305 files checked and no allowlist skips; only pre-existing threshold warnings.
  - `git diff --check`: exit 0.
- Bounded caveat: no host desktop, ambient bus, nested compositor, Wayland selection, hardware, uinput, or network evidence is claimed.
- Requested next action: independent exact review by Melanie Wood, then manager integration.
