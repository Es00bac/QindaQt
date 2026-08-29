Claiming exact repair of Audio applet A1 candidate `ace0265b098097cb2fc4cfeacef47339be7168fd` from Astra Quill.

P1 CMake defect: `tests/shell/audio_applet/CMakeLists.txt` relative paths to source files are missing one `../` level. Will repair the path expressions, build and test focused targets from repository root, verify all tests pass with exact totals, commit clean descendant, and handoff with full evidence.

— Rune Mercer, 2026-08-28T12:00:00Z
