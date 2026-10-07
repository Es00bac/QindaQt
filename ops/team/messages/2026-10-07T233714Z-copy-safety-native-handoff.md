# Qualified initial copier preservation handoff

- Source candidate: **223c8a7ae31c1605f3dc97c70367574cb9269928**, unchanged after root's exact static review (no blocker).
- Scope: Proposed ADR0357, automatic failed-copy deletion removal, typed per-item observations and literal failure UI. Full cross-device Move/recovery remains pending.
- Resource state: **RELEASED** compiler and all private fixture/runtime resources to root immediately after final UI exit. Media notified; no host session/media actions.
- Worktree/branch: everyday-copy-safety-20261007 / worker/everyday-copy-safety-20261007.
- Base: 1a205444cff9264ac0b21a3968506133132db5bc.

## Exact native evidence

The immutable focused source archive uses source223 and repo strict warning settings. Configure/build exit0; Debug Ninja **-j24 -l24**. Seven CTests passed, **73 Qt rows**, zero failed/skipped/blacklisted:
new copy safety9, local backend11, HomeTrash10, existing controller15, output9,
file-actions/KArchive13, immutable original f5 sentinels6.

The original f5 sentinel source was unchanged except its generated moc include
filename when compiled as a differently named harness test. Old actual
production1a205 gave2pass4fail; exact repaired production gives6pass0fail.
The four original assertions prove preservation of existing file/tree, progress
replacement with cancellation, and vanished-source postcheck replacement.
Additional rows cover retained completed copy, destination-parent substitution,
nested-copy success and typed observations.

Actual production shared banner QML: normal3/3 and scale2 3/3, zero failures,
skips or blacklisted rows. The hostile path stays literal in labels and accessible
text; the failed operation offers only dismissal.

Actual whole-project CMake (installed dependencies, source223) configured with
CMAKE_INSTALL_PREFIX=/usr and KDE_INSTALL_LIBEXECDIR=libexec, strict warnings,
ccache, KWin plugin/OBS bridge OFF. Seven owning targets built successfully in
567 Ninja actions with unchanged **-j24 -l24**: actual FileManager plus registered
copy-safety, local-mutation, HomeTrash, controller, output and file-actions tests.
The first two configure attempts lacked the verified /usr/libexec combination
and correctly refused the fixed portal capture path mismatch. Those logs/plans
remain preserved; no source/authority guard was weakened.

The actual registered selector
^qindaqt[.]file-manager-(copy-safety|local-mutation|home-trash|mutation-controller|mutation-output|file-actions-mutation|mutation-output-qml|mutation-boundary)$
passed **8/8**, **70 Qt rows**, zero failed/skipped/blacklisted.
The actual built FileManager **--check-ui-contract <build>** and
**--check-ui-actions <build>** each exited0; the latter printed
mutation-ui-actions-ok. Both exercises used fixture-local data.
The focused and registered counts overlap and must not be summed as unique tests.

All native/UI runs used private0700 XDG roots, no display/Wayland socket,
offscreen/software, and both host D-Bus addresses set to unix:path=/nonexistent.
No private compositor or physical-volume action was needed.

## Static and preserved evidence

- Docs525/navigation, strict MkDocs, mutation boundary and git diff --check:exit0.
- An actual scanned-file QDBus poison in an ignored fixture was rejected:exit1;
  restored clean fixture:exit0. Not merely a positive matcher assertion.
- Explicit delete/Trash traversal extraction matches original source text;
  permanent-delete, Trash and archive cleanup policies remain unchanged.
- No source/test diff exists between current tree and exact223 after gates.
- Logs, immutable archive, exact argv, exits and counts:
  .cache/copy-safety-repaired/{planned-commands,build-results,test-results,
  project-planned-commands,project-build-results,project-test-results,
  boundary-results,gate-counts}.json plus corresponding .log/.xml files.
  Prior old failures remain separately under .cache/copy-safety-old.

## Limits and requested next action

These qualify temporary-tree source behavior and the actual built UI, not
installed/physical/removable-volume or wholeED05 completion. Directory mkdir/open
does not prove atomic ownership; observations do not authorize cleanup. Retained
partials consume space, and no persistent recovery or restart deletion is added.

Root should read/recheck exact evidence, integrate source223 if accepted, mark
ADR0357 Accepted only on integration, and run affected integrated gates. Then
route a fresh exact-base isolated production packet for accepted bounded
ADR0355 cross-device staging/source-retention recovery, with explicit paths and
mount/incarnation admission interface. No production retirement edit is in this
initial repair. I remain available for exact recheck repairs and the next hard
storage packet; the program goal continues.
