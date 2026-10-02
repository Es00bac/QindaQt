# PermissionStore Settings candidate handoff

2026-10-02T18:20:07Z

Exact tested product source: `50b537c76c2d77d1cc0836dddd21fb09ae40ceea` on `worker/pf-portal-permissions-20261002`, from exact base `0990a59d913a58078681adc66d6dca96256407dc`. Isolated laptop WT `.cache/pf-portal-permissions-20261002` and qinda mirror `/home/cabewse/.cache/pf-portal-permissions-20261002`; both clean at test checkpoint. This receipt successor changes own operational records only.

## Outcome and changed paths

New `src/apps/settings/portal_permissions/` owns a bounded asynchronous GUI-thread PermissionStore model, standard session bus composition, and native Qinda Controls page. It lists existing frontend screencast/remote-desktop UUID singleton app grants, keeps restore data opaque, re-Lookup checks the sole selected app before standard Delete, addresses the unique store owner, invalidates stale replies, coalesces changes, and reports unavailable/refusal/malformed/oversized states without replay. Standard dormant service activation uses the existing D-Bus service; no new store, service, backend dependency, Settings1 persistence, or capture/remote-input edit.

Narrow additive `src/apps/settings_center/` route enum/registry/search metadata, active Loader and page-component bindings, and CMake links append route index22 while keeping indices0–21. `src/CMakeLists.txt` and `tests/CMakeLists.txt` append module/tests. New `tests/apps/settings/portal_permissions/` has private-bus wire fixtures and actual public-module QML Loader/controls gate; existing registry/navigation tests follow append and wrapping. Primary wiki `apps/portal-permissions-settings.md`, proposed ADR0339, reciprocal Settings Center/completeness inventory and MkDocs navigation reflect the boundary and limits. Own ops board/thread only otherwise.

## Actual gates

All compile/data gates on qinda; compiler grant used `-j2 -l12`, explicit `/usr` and `libexec`, frozen manager fork stage prefix, strict warnings enabled. Only four focused targets built. Final build exit0. Compiler slot released immediately.

`ctest --output-on-failure -R '^qindaqt[.](settings-portal-permissions-(store|page)|settings-route-registry|settings-navigation-controller)$'` exit0, 4/4 PASS, 0 failures, 1.77s.

- Store actual private D-Bus List/Lookup/Delete: QtTest6 PASS/0 FAIL/0 SKIP; unrelated resource/table preservation, refused delete, changed shared grant, >512-ID refusal, malformed reply, owner loss/late reply.
- Public QML module through active Loader: QtTest3 PASS/0 FAIL/0 SKIP; rendered revoke click, app key forwarded, unavailable notice and disabled action. Fatal Qt warnings enabled.
- Registry: QtTest10 PASS/0 FAIL/0 SKIP.
- Navigation: QtTest11 PASS/0 FAIL/0 SKIP; stable preceding indices and new wrap boundary.
- `python3 tools/docs_validation.py`: exit0,493 Markdown/nav documents.
- `mkdocs build --strict --site-dir .cache/wiki-site-final`: exit0,10.45s.
- `git diff --check BASE`: exit0.

qinda logs `.cache/build-final.log`, `.cache/tests-focused.log`, `build/dev/Testing/Temporary/LastTest.log`, `.cache/docs-links-final.log`, `.cache/wiki-build-final.log`. Focused CTest log SHA256 `cc5c3071435695b2038fb8204ef7e684697e392e9ef41de9586038a86dd3fd57`. First missing closed search-switch case and fixture reply-overload compiler failures were narrowly repaired and original logs preserved.

## Bounded caveats and next action

Manager directed SIGINT of only this worker's fresh1961-step full Settings dependency build at62 steps; preserved `.cache/build-route-interrupted.log`. Full executable `--page portal-permissions --route-construction-probe` remains the integrated desktop/package gate, with dedicated CTest entry present; no passing full executable or installed result is asserted here. Focused public module Loader proof and registered route proof are actual.

Private-bus configs omit service directories so focused gates cannot activate a real user PermissionStore. No live permission writes, installation, profile/world edits or GPU work. Revoke affects remembered persistent tokens, not active sessions or transient grants. Standard Delete has no compare-and-delete transaction; frontend UUID-per-app ownership remains necessary between fresh Lookup and Delete, and an active frontend session can save a grant again. Shared/non-grant entries are skipped; other tables/resources preserved.

Requested next action: different-worker exact-candidate review, then manager integration and warm integrated full Settings route gate. Available for bounded reproductions/repairs in this same WT; no further compiler or product outcome claimed while waiting.
