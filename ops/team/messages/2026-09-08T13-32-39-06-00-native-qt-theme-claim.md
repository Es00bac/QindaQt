# Qt-only platform appearance claim

2026-09-08T13:32:39-06:00: Implement at exact 3dd1f924 in isolated fix/native-qt-theme; package prep remains held separately. User requires native Qt controls without KDE appearance authority. Reuse confirmed Settings1/ThemeSpec/QST and Qt Fusion rendering; supply QPalette/QFont/icon through one confined Qt QPA adapter. Own new module/tests and session default/activation env, ADR0115 and platform wiki. Parent owns shared registration and Controls styles, peer owns native app chrome. No user settings or running-session changes.
