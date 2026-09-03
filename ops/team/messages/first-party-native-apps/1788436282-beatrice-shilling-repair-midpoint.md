# Beatrice Shilling — Text Editor S2 repair midpoint

- Timestamp: 2026-09-03T05:51:22-06:00
- P1-1: new `saveAsResolvesSymlinkedParent` regression preserves one controller after Save As through a parent alias and rejects an uncanonicalizable parent.
- P1-2: new `symlinkedAncestorCannotEscapeStateRoot` regression proves component-by-component `openat`/`O_NOFOLLOW` traversal refuses the escaping `qindaqt` ancestor and creates no external file.
- P2-1: new `titleCollapseHonorsUtf16Boundary` regression budgets collapsed whitespace with the following scalar and retains valid UTF-16.
- P2-2: the two CLI and installed-package rows now register `QT_FATAL_WARNINGS=1`; `qindaqt.editor-offscreen-warning-policy` checks the generated registry and its checker exits 1 against the rejected candidate registry.
- Verification midpoint: strict Debug and Release focused builds pass; each profile passes editor 16/16 and Settings schema/migration 2/2 under build-root HOME/XDG/TMP and disconnected buses. Documentation, strict MkDocs, source-shape, JSON, and diff gates pass.
- Next: final diff audit, product commit, immutable candidate replay, and exact handoff.
