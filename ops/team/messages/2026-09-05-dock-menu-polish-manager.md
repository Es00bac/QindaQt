# Dock and menu polish integration queue

Base: `5771c97644db5e48e622ab71154e4155c10c6714`.

- dock_polish: `.cache/polish-dock`, task-list and pinned QuickLaunch opt-in dock tiles; runtime controls and pointer/keyboard behavior stay functional.
- panels_polish: `.cache/polish-panels`, common panel material/spacing/content bounds, stock QindaQt/Mac profiles and dock forwarding. Owns panel window input-mask integration if needed.
- menu_handoff: `.cache/polish-menu`, first-party menu export/host availability and local-menu fallback. A running composition alone must not be mistaken for a live global-menu renderer.

The manager integrates reviewed exact candidates, rebuilds the combined tree,
and runs focused plus private visual checks. The design uses existing tokens,
accessibility preferences and persistent pinned-app controls. No new package
installation, security redesign, or memory-optimization gate is part of this pass.

Acceptance: compact icon-based dock with truthful running state, pinned-launcher
operations, readable hover feedback, bounded magnification, non-overlapping
spacing and real translucency with opaque accessibility fallback; no duplicate
local/global application menus, with local actions restored on host loss.

## Integration note

Dock and panel candidates were independently reviewed and integrated through
`65ce9025`. Before integration, the shared tree contained a matching profile
layout with a 72-pixel dock rather than the candidate's 80 pixels. That local
profile content was preserved byte-for-byte through a targeted stash and saved
patch; only the reviewed QML/runtime implementation was added around it. The
72-pixel surface has a 64-pixel content row for the 60-pixel tile, and is the
actual geometry under combined/runtime verification. Documentation tracks it.

The host /tmp tmpfs is full; new manager build temporaries use the repository's
ignored disk-backed `.cache/build-tmp` directory. No unrelated temporary data
was removed and no package installation is needed.

## 2026-09-05T22:34:36.777142+00:00 — Integrated verification and review corrections

Panel geometry passed 20/20 with the Qt 6.11 runner. The initial panel review conjecture about empty spacing tokens was incorrect: the runner imports a seeded Tokens stub. The actual missing stub roles and launcher dock dimensions were repaired and independently reviewed. Dock focused selection initially passed 28/29; the remaining test now passes after its delegate lookup follows the visual tree, preserving all assertions. Strict documentation build passes. Dock-only nested 1080p and package fixture pass 2/2. Menu integration remains pending lifecycle repair and review.

## 2026-09-05T22:44:47.308407+00:00 — Menu integration and broad findings

Menu candidate 23956be2 passed independent review and is integrated with ADR-0077. Broad dock/panel verification passed 640/644: the duplicate QindaQt launcher was repaired by 0bf27df1 and integrated resolution passes; the display private-bus test passes using a shorter disk-backed TMPDIR; two desktop-control fixture failures were sent to their owner. Integrated menu selection passed 33/35, with two old registrar fixtures subsequently repaired in a96979fe (independently accepted; integrated rerun pending). Private screenshots confirm the Editor local menu disappears after hosting. Final active global-menu capture and launcher Pin interaction remain pending.

## 2026-09-05T23:19:12.328607+00:00 — Final acceptance evidence

The final direct Pin/Unpin UI replaces the attempted context-menu UI. Full-width nested captures exposed the real narrow-row hit-region bug; the evidence does not establish a TapHandler or compositor delivery defect. The viewport-width repair, compact token buttons, authoritative pin resynchronization tests, and actual 40-pixel Quick Launch icon geometry are integrated through bb4a233d. Independent review accepted each production candidate and repaired test descendant.

Final affected selector: 46/46, followed by the icon geometry row 1/1. All four failures from the earlier broad 640/644 run are closed by passing focused repairs/reruns. Full Debug build, final nested 1080p/package 2/2, display/profile matrix 6/6, strict MkDocs, 169-document link validation, and source-shape gate pass; source shape reports 13 decomposition warnings: 12 existing and the launcher QML test now at 543 nonblank lines. Independent review of bb4a233d accepts its cohesive shared fixture and separate behavior slots without a blocking split; revisit decomposition before further growth toward 600 lines. Private run ecb6c8693a8e3efe259cd86af2c2b663 verifies menu placement, outward menus, and Settings-to-Editor activation through the transparent dock margin. Run ff90df2d9b8e1a8a28a24d2fc1a14862 verifies real primary Pin/Unpin effects and readable full-width rows. Screenshots and generated logs remain ignored, not committed.


## 2026-09-05T23:23:31.292864+00:00 — Final desktop capture

Actual harness run `767246f24aba33ef7a7705ee4f45c223` completed successfully with return code 0 and no survivor PIDs. The final screenshot shows the active Editor menu only in the global bar and the pinned Quick Launch icon at its corrected 40-pixel size. The worker label `bb4a233d-final-pin-editor-20260905T2318` is a descriptive label, not the harness run ID. The private audit hook matches its baseline SHA-256 byte-for-byte.
