# Everyday storage audit receipt

- Timestamp: 2026-10-07T16:40:00Z
- Auditor: everyday-storage-auditor-sol
- Base: f298b680375fd6d759b877803e8a88d71346d5d8
- Branch: audit/everyday-storage-20261007
- Scope: read-only source/installed metadata audit; no builds, tests, mounts, ejections, file mutations, installs or private-document inspection.

## Evidence and claim boundaries

The user's inability to carry out ordinary thumb-drive workflows without a terminal is a user report. This audit did not reproduce it. The GUI implementation is present and installed; whether the deployed notification/polkit/device journey actually works remains unknown.

Removable Media is a separate first-party application, not File Manager's disk authority. Source implements insertion notification actions, Mount and open, read-only mount, encrypted unlock, remembered choices, Unmount, Safely remove/Eject and captured-device format confirmation: src/apps/removable_media/media_controller.cpp:108, MediaDetails.qml:49, MediaDetails.qml:72; docs/wiki/apps/removable-media.md. Production supervisor configures it (src/session_supervisor/app/main.cpp:147); --watch startup is explicit (src/session_supervisor/src/session_process_supervisor.cpp:70, :452). main.cpp:177 starts File Manager for the mounted path with literal argv and exposes a retryable launch error.

Direct read-only evidence on the laptop: command -v returned /usr/bin/qindaqt-removable-media, /usr/bin/qindaqt-file-manager and /usr/bin/qqterm. Portage records exist for gui-apps/qindaqt-removable-media-0.1.0_p20260930-r2 and sys-fs/udisks-2.11.2. ps -eo comm showed qindaqt-session, qindaqt-polkit-, qindaqt-removab and udisksd (comm names may truncate). On qinda the executables and package records also exist; the process-name filter showed polkitd only. Presence/residency is not device-operation success or package/source equivalence.

File Manager implements extensive everyday local browsing/mutation, clipboard, four views, local archive compression/extraction, Open With, defaults, remote SMB/SFTP locations and local/remote transfer queue. Source has bounded intentional limitations below; these are confirmed source gaps against the desired user workflow, not unexplained regressions.

## Prioritized bounded outcomes

1. **P0: Qualify the deployed thumb-drive journey. Implemented but unverified; user-reported failure.** First reproduce on the installed laptop using disposable media: insertion at login/after login, notification Mount and open, launcher fallback, polkit, read-only, busy eject, unplug/replug and safely remove. Fix only the reproduced boundary. Acceptance: all ordinary actions work with pointer/keyboard and no terminal prerequisite; busy/denied/owner-loss results remain visible without forced removal. Existing synthetic gates alone cannot establish hardware success.

2. **P1: Show connected/mounted volumes and removal actions in File Manager. Absent first-party sidebar integration.** PlacesController::places() enumerates fixed Home/File System/Trash/Recents/Applications/Network entries (src/apps/file_manager/model/places_controller.cpp:42); PlacesSidebar.qml:101 consumes them, with no device section. ADR-0315 explicitly defers volume sidebar integration. Add a read-only inventory/public action seam to the existing media owner, preserving UDisks separation. Acceptance: newly attached media appears with truthful mount/busy state; mount/open and safe removal are reachable from the browser, and vanished/replaced devices cannot receive stale actions.

3. **P1: Make Cut/Move work between the internal filesystem and USB storage. Confirmed unsupported source contract.** src/apps/file_manager/mutation/local_mutation_backend.cpp:249 returns CrossDevice and “Moving across filesystems is not supported”; docs/wiki/apps/file-manager.md:423/:523. Add bounded copy-then-delete with explicit failure/recovery semantics. Acceptance: a cross-device move verifies destination before source removal, cancellation/full device preserves source, conflicts never overwrite silently, and successful cut-paste clears clipboard only after commit.

4. **P1: Recoverably delete files on mounted volumes. Confirmed absent per-volume Trash.** home_trash.cpp:205 returns CrossDevice (“it was not deleted”) for another filesystem; docs/wiki/apps/file-manager.md:527/:542. Add specification-compliant per-volume Trash behind volume identity, not a permanent-delete fallback. Acceptance: Delete on disposable USB moves to that volume's Trash, Put Back survives restart and mount/reconnect, denied/read-only media shows an actionable refusal, original contents remain safe on failure.

5. **P2: Copy ordinary folder trees containing symbolic links. Confirmed source limitation.** safe_tree_operations.cpp:318 refuses a symbolic link anywhere inside a copy; File Manager can create links (mutation_controller_items.cpp:115), so a generated link subsequently blocks copying its containing folder. Extend copy to preserve link objects without traversal. Acceptance: relative/absolute/dangling links copy as links; hostile replacement/out-of-root targets are never followed; cancellation cleans only newly created destinations.

6. **P2: Make supported remote actions discoverable in the same context menu. Confirmed UI parity gap.** FileContextMenu.qml:44/:45 excludes remoteActive from localFolder; docs/wiki/apps/file-manager.md:457 says the right-click set is local-only despite existing remote rename/new-folder/copy/move and transfer seams (docs:798 onward). Expose only already-supported remote actions through existing capability/lineage gates. Acceptance: SMB/SFTP keyboard/right-click actions match supported menu/toolbar actions; unsupported local-only operations stay visibly explained and no stale selection reaches a replacement listing. Do not claim network browsing, authentication or remote write-back is absent.

7. **P2: Offer recoverable partial-batch results and missing-parent restoration. Confirmed bounded recovery limits.** docs/wiki/apps/file-manager.md:490 and :1537 specify no batch undo/Restore Last; mutation_controller_items.cpp:175 refuses Put Back when original parent is gone. Define one bounded recovery UI that identifies completed/failed items and lets users choose an existing restore folder without terminal work. Acceptance: mixed-success/cancelled batches show completed count and paths; restore-to-choice never overwrites; restarting retains Trash records. A full durable operation journal is a separate architectural decision.

## Archives, defaults and terminal parity

Archives are implemented inside File Manager: Compress writes zip; Extract reads zip/tar with gzip/bzip2/xz/zstd, cancellable through KArchiveCodec and safety bounds (docs/wiki/apps/file-manager.md:637; mutation/karchive_codec.cpp and karchive_codec_extract.cpp). A standalone archive application is a different product claim: this audit did not inspect whether QindaArk or external Ark is installed. Correct stale handbook text to “File Manager browses SMB/SFTP network locations and performs supported remote operations and queued local/network transfers. It compresses to zip and extracts supported zip/tar archives; this does not establish a separately shipped archive-manager application.”

Open With and default applications are present, including Settings category choices and per-type Always Open With using one shared store (docs/wiki/apps/default-applications.md; docs/wiki/apps/file-manager.md:595). D-Bus-activatable Open With and non-zip compression remain explicit limited scope (docs:1543). These are lower priority than the seven outcomes above.

Terminal parity bridge exists: Open Terminal Here runs qqterm --working-directory, and Terminal=true handlers run qqterm -e (docs/wiki/apps/file-manager.md:615; model/open_with_launcher.cpp:40). QQ_Term feature/performance parity belongs to the separate terminal audit. Removable Media exports only Activate; the lack of first-party disk-mutating CLI is an intentional boundary, not evidence users must mount in a terminal.

## Existing gates and verification record

Proposed rerun commands, not executed in this audit:
- ctest --test-dir <build> -R '^qindaqt\.(removable-media-|session-removable-media-lifetime)' --output-on-failure
- ctest --test-dir <build> -R '^qindaqt\.file-manager-' --output-on-failure
- ctest --test-dir <build> -R '^(qindaqt\.settings-default-apps-|session\.sessiondefaults)' --output-on-failure

Gate definitions: src/apps/removable_media/CMakeLists.txt:45; tests/session_supervisor/CMakeLists.txt:181; tests/apps/file_manager/CMakeLists.txt; owning app docs' verification sections. Fixture policy/UDisks/notifications/offscreen gates are available but do not qualify actual mount/eject, live network transfer, live default-handler launch or assistive technology. No test pass/count claimed.

Inspection commands: ssh qinda with git status --short, cat AGENTS.md and owning docs, rg/rg --files and nl/sed for anchored source; local/remote command -v, package-record filename listing and ps -eo comm. Successful inspected batches returned 0 unless noted. Two path searches returned missing-path/glob errors and were corrected against rg --files; no evidence is derived from those failures. After permission-profile change, ordinary SSH failed config access and configuration-free SSH failed hostname resolution; authorized automatic escalation restored SSH.

## Requested next action

Manager: review exact docs candidate, synthesize P0 installed reproduction first and independently scoped File Manager tasks. No product milestone advances from this audit. Physical storage and network/default-handler qualification remain explicitly unperformed.
