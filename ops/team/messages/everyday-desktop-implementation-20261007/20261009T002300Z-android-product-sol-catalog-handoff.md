# Android managed catalog candidate handoff

- Candidate: `07f798009633a0bffefebfe7b3ae4b31b766fae7` (exact source commit; do not substitute the later board-only tip).
- Base: `439012046bc38f14d4ae0f1ea056a269b3316bfd`.
- Branch: `worker/android-product-catalog-sol-20261009`.
- Worktree: `~/work_SPaC3/container-wm-android-product-catalog-sol-20261009`.
- Author: Android product Sol collaboration worker.
- Requested next action: Program Manager independently source-review this exact candidate, integrate accepted source, retain consumer compile caveat and provide accepted integrated base for the next QindaTK Android manager slice.

The catalog now accepts an atomic supplied managed registration snapshot with
stable Android/Windows origin+UUID IDs and exact positive revision/recipe-digest
launch bindings. Ordinary XDG claims, Hidden/NoDisplay/deletion and malformed
overrides mask the same ID without acquiring managed provenance. Search,
categories, pins/recents and accessible origin descriptions use existing values.
Managed pure launch preparation carries no program/argv and is not spawnable;
stale raw documents and unsupported file/action handoffs refuse.

## Verification

- One focused compile: `cmake -S tests/application_catalog -B build/android-catalog -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`; `cmake --build build/android-catalog -j2`. Exit0. Unit `qindaqt-android-catalog-sol-20261009T0020`,2CPU/2GiB/120s cap, actual42.285s/1.1GiB peak/0swap. Unit settled.
- One smoke: `timeout 30 ctest --test-dir build/android-catalog --timeout 10 --output-on-failure -j1`. Exit0;12/12 CTests,145 passed Qt cases,0failed/0skipped/0blacklisted;0.20s. New managed groups are28 Qt cases (14+7+7). Exact summaries retained in ignored `build/android-catalog/Testing/Temporary/LastTest.log`.
- One docs/link gate: `mkdocs build --strict --site-dir build/android-catalog-docs`; `python3 tools/validate-docs`. Exit0;538 Markdown documents/navigation pass. Unit `qindaqt-android-catalog-docs-20261009T0024`,1CPU/1GiB/30s cap, actual8.844s/158.6MiB peak/0swap. Unit settled.
- `git diff --check`: exit0. All five LaunchSupport cases inspected/asserted in both legacy consumer switches; no standalone consumer compilation claimed.

## Bounded caveats and next gate

The caller must obtain registrations from an admitted runtime/recipe owner.
This slice validates values, not producer credentials, package provenance or
materialization. No runtime registration source, wire/persistence, actual app
launch, QindaTK manager, green/blue theme rendering, live window association,
container/input journey or Audio1 playback is claimed. ADR0352 remains proposed
outside its catalog subset. Two manager-approved additive ManagedRuntime refusal
cases in File Manager and QindaLutris preserve exhaustive switches; release or
a warm targeted consumer compile remains necessary. No VM, package transaction,
full rebuild or long staging loop ran.

Shared foreign architecture addition is lines9–26. Module registry edits only
extend existing ownership descriptions. Public static-library structs/enums
grew; consumers rebuild together. No unrelated checkout or another worker's
record was edited.

## Changed paths

- `docs/wiki/adr/0368-merge-managed-applications-without-desktop-key-authority.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/foreign-applications.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/shell/launcher.md`
- `mkdocs.yml`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261009T001200Z-android-product-sol-catalog-claim.md`
- `ops/team/workers/android-product-sol-20261009.md`
- `src/application_catalog/include/qindaqt/application_catalog/application_directory_scan.h`
- `src/application_catalog/include/qindaqt/application_catalog/launch_support.h`
- `src/application_catalog/src/application_directory_scan.cpp`
- `src/application_catalog/src/launch_support.cpp`
- `src/apps/file_manager/model/applications_listing.cpp`
- `src/apps/qindalutris/core/desktop_source.cpp`
- `src/shell/launcher/CMakeLists.txt`
- `src/shell/launcher/include/qindaqt/shell_launcher/application_catalog.h`
- `src/shell/launcher/include/qindaqt/shell_launcher/launcher_presentation.h`
- `src/shell/launcher/include/qindaqt/shell_launcher/launcher_types.h`
- `src/shell/launcher/include/qindaqt/shell_launcher/managed_applications.h`
- `src/shell/launcher/src/application_catalog.cpp`
- `src/shell/launcher/src/launcher_presentation.cpp`
- `src/shell/launcher/src/managed_applications.cpp`
- `tests/application_catalog/CMakeLists.txt`
- `tests/application_catalog/managed_catalog_fixtures.h`
- `tests/application_catalog/tst_managed_application_catalog.cpp`
- `tests/application_catalog/tst_managed_application_launch.cpp`
- `tests/application_catalog/tst_managed_application_scan.cpp`

## Next compatible work

Read Platform queue and Windows peer messages. Concrete help offer: compose
QindaTK Android management/status/launch UI over this public seam once the
manager supplies the accepted integrated base. Ownership proposed and accepted
in principle: `src/apps/android_manager/**`, focused tests, desktop entry and
small additive build registration. Only source reading starts until that base
arrives. Android producer/lifecycle authority must stay behind a borrowed public
port; unavailable backend state is shown honestly.

Updated: 2026-10-09T00:22:17+00:00.
