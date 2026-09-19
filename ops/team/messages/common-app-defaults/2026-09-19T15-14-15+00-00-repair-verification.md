# Repair verification — NoDisplay MIME lookup

- Time: 2026-09-19T15:14:15+00:00
- Repair to product candidate: `6967d76cd792379094a3fd3d3a0342cf7cc06730`
- Public contract: existing `ApplicationCatalog::build(documents)` and `scanApplicationDirectories(roots,error)` signatures and menu-only behavior remain. Explicit overloads accept `ApplicationVisibility::IncludeNoDisplay`. Appended `ParsedDesktopEntry::deleted` records exact Hidden=true while original aggregate hidden semantics remain. Static-library consumers rebuild after public-value changes.
- Settings composition requests IncludeNoDisplay. Hidden/deleted and malformed first-root documents still mask lower-root copies before filtering; source ceilings, validation, retained raw bytes and original menu scans are unchanged.
- Focused and adjacent target builds (-j2): exit 0.
- Defaults/session plus pure launcher/public catalog/boundary CTest selection: exit 0, 19/19. Parser 43, L0 catalog 14, public scanner 6, store 23 QtTest passes including lifecycle cases. Existing category/search/pinned/presentation and launch-support tests passed.
- Launcher runtime scanner/executor CTest: exit 0, 2/2; unchanged menu and hidden-first behavior remained green.
- Copied review probe (only injection changed to the production IncludeNoDisplay scan mode) in `build/defaults/nodisplay-review-probe`: exit 0, `settingsBrowser=nodisplay.desktop` and `xdgBrowser=nodisplay.desktop`.
- `tools/validate-docs`: exit 0, 332 documents. Strict MkDocs and diff check: exit 0.
- Optional `tools/check-source-shape`: exit 1, 16 errors in 15 paths. Every reported error path is byte-unchanged relative to exact assignment base `de8786e15e5ad8e8b9b63c49866f4b284f5c7bce`; no change-owned shape finding. No unrelated files were repaired.
- Remaining bounded follow-up: pre-existing any-MIME candidate vs representative-MIME validation mismatch remains nonblocking per reviewer/manager; combined installed-route/deployed launch remains manager-owned.
- Next action: same reviewer checks the repaired descendant; manager updates shared ADR/module-boundary docs and integrates only after that verdict.
