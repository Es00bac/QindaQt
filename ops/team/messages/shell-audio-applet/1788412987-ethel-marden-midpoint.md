# Ethel Marden midpoint — staging closure repaired

- Time: 2026-09-02T23:23:07-06:00
- Exact base: `24063dd0dda228ee99adfda0bd6ae04cc19cdc4e`
- Reproduction: Debug `^qindaqt\.(power|bluetooth)-applet-installed-package$` failed 0/2 because both staged shells could not load `libqindaqt_controls_qml.so`.
- Repair: each narrow Audio, Power, and Bluetooth shell component now stages `qindaqt_controls_qml` beside the shell's libdir and `qindaqt_tokens_qml` at Controls' baked `$ORIGIN/../Tokens` destination. The Power and Bluetooth package rows authenticate resolution from those relocated artifacts before source-poison launch.
- First post-fix evidence: Debug `^qindaqt\.(audio|power|bluetooth)-applet-installed-package$` passed 3/3.
- Remaining work: complete focused builds, full applet/integrity/shell-runtime selectors in Debug and Release, static gates, immutable product commit, and handoff records.
