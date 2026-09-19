# Candidate review repairs and desktop integration

- Observed:2026-09-19T15:18:08Z.
- Main includes Opus launcher-focus commit500b1de5; remote uncommitted package r3 preserved. Viewer ADR is now0218 to avoid the new remote ADR0217.
- Defaults812b6cf7 repairs valid NoDisplay file-handler visibility through the public catalog while preserving menu consumers. Same reviewer rechecks exact source.
- Viewer f56b9a70 was rejected for a demonstrated Latin1 password encoding defect; same implementer owns repair. Four ordinary gates and visual layouts passed independent review.
- QINDAQT_BUILD_VIEWER defaults ON and remains mandatory for full packages; hosted CI explicitly excludes it while QindaTK has only local source hosting. Native viewer gates remain required.
- Final remote build will consume the accepted combined commit; the current isolated baseline compile is only cache preparation. No live install or user preference edit.
