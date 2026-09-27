# Shell performance claim and finding

Base `64ae95c336c03a2de54775962ff42622fae1d156`; worker `shell-performance-codex`; isolated branch `fix/shell-icon-performance`.

The manager supplied live profiling with `realpath` dominating shell CPU and `QV4::Sequence::virtualGetLength` calling shell getters. Source inspection confirms `entryRows()` and `windowRows()` rebuild the complete row list, including filesystem-backed icon resolution, on every read. A QML loop reads length and each element repeatedly; dock magnification can amplify this work with the number of windows. Proposed fix materializes both existing bounded projections once before `stateReprojected`, with no path-cache/security policy change. Acceptance: resolver call counts stay constant during repeated native/QML reads; metadata and revisions refresh atomically after new source generations; focused task-list suite and docs gates pass. Manager owns integration, live proof and packages.
