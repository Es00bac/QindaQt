# File Manager viewport worker

- Identity: file-manager-viewport (OpenAI Codex)
- Status: waiting — Qt-only adapter candidate ready for independent review
- Base: 3dd1f924c46d04ee20074207d368b8bc468aa8d6
- Worktree: .cache/native-qt-theme / fix/native-qt-theme
- Ownership: src/platform/qt_theme, tests/platform/qt_theme, session appearance environment, ADR0115/platform wiki

## Updates

- 2026-09-08T13:32:39-06:00 — User explicitly rejected KDE appearance authority; earlier audit recommendation superseded. Implemented ordinary Qt QPA palette/font/icon adapter backed by existing Settings1/AppAppearance/QST, with built-in Fusion style and Qt generic platform service delegation. No QStyle, native-dialog, tray or KDE configuration reimplementation. Focused isolated Qt-only build underway.
- 2026-09-08T13:43:57-06:00 — Qt-only focused build and five CTest rows pass; native Widgets/Quick live palette/font, explicit style, relocated plugin, native-service delegation and session overrides. MkDocs strict plus 217-document link/navigation gate pass using parent registry overlay. Existing skins untouched; native Wayland interaction is a bounded combined-session caveat.
