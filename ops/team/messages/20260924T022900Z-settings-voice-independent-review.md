# A01 Voice independent review

Reviewed exact candidate HEAD 6e52f83af2a4a306d10c1dc2296aa9eb4a091300 against its base 7ba705bf53fde8887ca38c89ad24c85b3823ee68 and current main d9cd43f570b670cf29bb0e97a7a3117316ccd71f. The Voice route, console, shell applet, gate, focused tests, and Voice docs trees are byte-for-byte equal between candidate and current main, so the Voice product result is already present on main (main has the equivalent Voice implementation commit 6a2d9e2d). Verdict: accepted; no blocking finding.

Contract review:
- The shared VoiceInputPreferenceGate admits only an exact Boolean true from a scoped snapshot whose owner matches SettingsClient currentOwner and while SettingsClient is Ready or Authenticating. Missing, malformed, default Off, unavailable, or owner-replaced state is closed.
- The shell composition and Voice console start Voice1 only through that gate; retry paths check it, and popup expansion cannot start the client. Gate loss stops the client. The Settings route synchronizes Voice1 with the same gate and has a local Off withdrawal before sending the write.
- Two-key commits proceed one key at a time and wait for a snapshot at or beyond the committed revision before sending the next key. Contradictory/uncertain replies are not replayed. Owner/epoch replacement clears the old draft; same-owner uncertain writes preserve the requested draft for review only.
- QML control enablement follows model predicates, and the docs describe default Off, provider separation, two-key readback, and failure behavior.

Verification: on qinda candidate worktree, ctest --test-dir build/settings-voice -R 'qindaqt\.(settings-voice|voice-applet)' --output-on-failure passed 3/3: qindaqt.settings-voice, qindaqt.voice-applet, and qindaqt.voice-applet-composition. Build targets and executables were already present; no source/build mutation was needed.

Current-main merge compatibility: git merge-tree --write-tree d9cd43f5 6e52f83a reports exactly two content conflicts: docs/wiki/adr/index.md and mkdocs.yml. Both are shared registries with additive entries from the independent notification-policy lane and Voice lane. Resolve each by retaining both additions (ADR-0244 entry/link and notification ADR entry; Voice Settings navigation entry and notification navigation entry), preserving existing ordering conventions. The merge tree auto-merges src/CMakeLists.txt, src/services/settings_client/include/qindaqt/services/settings_client/settings_client.h, src/shell/CMakeLists.txt, src/shell/runtime/shellruntimeapplication_applets.cpp, tests/CMakeLists.txt, and tests/services/settings_client/tst_settings_client.cpp. Since current main already has the Voice product tree, do not reapply duplicate Voice source changes.

No candidate or main source files were edited for this review; this timestamped reply is the required board record.
