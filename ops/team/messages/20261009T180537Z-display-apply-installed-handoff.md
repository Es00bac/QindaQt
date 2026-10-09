# Installed display Apply repair

- Timestamp: 2026-10-09T18:05:37+00:00
- Exact product candidate: `927039197bbfaae44ddadf1d5e22e47eb7d1c04f`
- Reviewer: `codex-display-apply-review-20261009`, exact ACCEPT
- Product paths: one private display runtime source file, existing native safety regression/CMake, affected Display Settings/service wiki pages
- Source branch: `fix/display-apply-20261009`, preserved on qinda; owner main-merge approval pending
- Overlay: `a13d49f`, integrated and synced
- Installed package: `gui-wm/qindaqt-desktop-0.1.0_pre20261002-r25`, normal Portage, laptop only
- gpkg SHA256: `e50efa433b6574a25814a6df54ac106aca221361cc4273fb6796d9e1d60273ea`

Evidence: focused C++/QML build exit 0; CTest 8/8 exit 0 in 2.97 seconds; documentation validation 551 and strict MkDocs exit 0. Private real native compositor with two virtual outputs applies position, 125% scale and 90-degree rotation, Confirm retains the layout and Cancel restores it (three checks, exit 0). Full gpkg signature, exact embedded recipe/source and all 2,622 image paths verified, exactly Display1 executable/runtime archive changed. Normal laptop Portage dependency plan and install exit 0. Display1 restart preserves original layout; live two-connected-monitor position preview reaches confirmation and Cancel restores the exact original layout (two checks, exit 0, service active).

Bounded caveat: the private compositor must finish cold startup before the existing 1.5-second native receipt request; no shared native transport timeout policy is changed. No physical rotation/scaling or qinda host installation is claimed. Temporary diagnostics were removed, and the verified product source is byte-identical to the accepted candidate.

Requested next action: owner authorization for source main integration; automatic approval review rejected that branch mutation because it could not establish manager authorization. The complete installed fix is active and user preferences are preserved. Root offers a concrete reproduction/repair recheck if an ordinary display operation still fails.
