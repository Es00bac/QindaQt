# Agent usage UI final verification handoff

- Source candidate: `341cc219dcc64ce030d9c0be9c2c74ac880f9cb1`; this receipt is a board-only descendant, final exact hash delivered to manager.
- Original base: `ccc99fb3f67c2bdc4f7156af7c0b0f1e05478057`. Inherits manager ADR `a9746fb40131e6fc9f609412eeba87b5d0e48988`, repaired backend `f9c6e76ee35c12c355fa75bd4edf0b059cd20803` and backend canonical board `61e6391a07287a691157dba2841b4b3132c799f2` verbatim (backend Qt18/Python3).
- Owned module paths: `src/shell/agent_usage_applet/`, `src/shell/runtime/agentusageappletcomposition.{h,cpp}`, `tests/shell/agent_usage_applet/`, `data/applets/agent-usage.json`, `docs/wiki/shell/agent-usage-applet.md`, own worker/message records.
- Coordinated shared paths: source/test/shell CMake registration; runtime composition/panel factory/context and compiled host/dispatcher facade properties; applet capability enum/token parser and builtin registry; `data/applet-policy/default.json`; all11 `data/profiles` usage placements; manifest/catalog/runtime resolution tests; shell icon inventory and Breeze fixture; own MkDocs navigation row. Existing behavior/design is preserved. Full changed-path inventory relative to original base is in ignored `build-agent-usage/candidate-paths.txt` and includes inherited manager/backend work.

## Evidence

- Strict Debug native `qindaqt-shell` plus focused UI/catalog targets build exit0, `cmake --build build-agent-usage --target ... -- -j24 -l24`; native `/usr` KDE layout and `libexec`. Final incremental log `build-agent-usage/build-final.log`; repaired native source link already passes in preceding source batch.
- Private HOME/XDG/bus normal focused CTest **8/8 PASS**, exit0, 0.67s, `build-agent-usage/tests-final.log`. Controller Qt8; QML Qt7, no failures/skips.
- Compact DPI2 compiled offscreen **1/1 PASS**, exit0, 1.28s, `build-agent-usage/tests-final-dpi2.log`. Actual Space open -> Tab to Refresh -> ArrowDown reaches Mistral/footer. Real mapped window wheel events reach both; increasing timestamps prevent the fixture presenting every wheel tick at the same instant. Earlier zero-timestamp failure logs remain ignored. No production scrolling behavior was added.
- Strict MkDocs and repository checker exit0, **519 Markdown documents/nav**, `build-agent-usage/docs-final.log`; `git diff --check` exit0.
- Native captures `build-agent-usage/evidence/compact/{usage-horizontal,usage-vertical}.png` and `compact/dpi2/{usage-horizontal,usage-vertical}.png`; manager visually passed ordinary/DPI2 compact layout. Captures are actual QQuickWindow frames, no generated mockup.

## Next action and limits

Compiler lease released to independent reviewer for exact frozen candidate. Request exact source review, accelerated actual while-open Timer/no-refresh event check and manager integration gates after acceptance. No installation, live session restart, physical service mutation or account limit qualification performed. Usage depends on actual provider metadata/reports; unsupported facts remain explicitly unknown. Canonical worker state is waiting with concrete exact-source repair/help offer; queue and peer handoffs reviewed, manager owns the current next gate.
