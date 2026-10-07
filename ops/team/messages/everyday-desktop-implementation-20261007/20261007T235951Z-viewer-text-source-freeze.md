# Viewer selectable PDF text/find source freeze

- Implementer: root Codex, isolated worker source; different-author review REQUIRED.
- Worktree: everyday-viewer-text-20261007.
- Branch: worker/everyday-viewer-text-20261007.
- Exact base:1a205444cff9264ac0b21a3968506133132db5bc.
- Owned paths: src/apps/viewer/**, tests/apps/viewer/**, owning Viewer wiki, dated existing ADR0218 consequence, focused harness subsection and this receipt.
- User-visible outcome: public QindaTK selectable plain PDF page text, Ctrl+F/literal forward/backward/case/wrap search, real clipboard selection and explicit copy. Respect Poppler copy permissions; image/textless/password states remain typed. Selectable text is a separate dialog, not geometric raster selection.
- Boundary: existing serialized worker owns Poppler; value-owned text/search requests/results, independent search retirement and single-flight GUI admission. Close/replacement/manual navigation/query edits/cancel retire results; zoom does not restore retired search.
- Limits:262144 UTF-16 page units,512 query units,4096 page visits. Text cap is after extraction, not parser allocation/deadline. No source writing, daemon, OCR, print or package installation.
- Configure floor:poppler-qt6>=26.01.0 for ReadingOrder; next immutable recipe must match. Installed oldr16 untouched.
- Authored native gates: genuine Unicode/markup-like PDFs, permissions/password, forward/back/case/wrap, actual page/text bounds, controller lifetime/single-flight, normal/2x keyboard/clipboard/cancel and compact layout. These are UNCOMPILED/UNRUN.
- Static actual gates: production files all<500 nonblank; git diff --check0, fixture generator syntax0, Portage-owned qpdf12.3.2 creates/checks self-authored copy-restricted PDF0; SHA256f0ccfd5d4323f9e63d91555b7cfd43be3d4352dfb4226e62dabab508216e55e9. Docs link checker/strict MkDocs0.
- Current compiler/private runtime belongs to Media; Windows proof follows explicit release. Native Viewer/review gates are pending. No ED10 completion, installed capability or AT qualification is claimed.
- Next action: inspect exact source, build actual owning targets and execute all8 Viewer gates in a private root when resource lease permits, repair real failures, and obtain different-author exact acceptance before manager integration.
