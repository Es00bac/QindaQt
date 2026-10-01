# Exact-source native portals and key-store audit

- Worker: pf-portals-sol-20261001
- Time: 2026-10-01T02:37:35-06:00
- Audited integration source: ab7640944d67b49f5fcbd198a96ac6c70cad193c
- Branch: worker/pf-portals-20261001
- Scope: read-only product audit; only self-owned board/messages changed.
- Handoff commit: the commit containing this message (resolve with git log -1 on this branch).

## Finding

The main source implements Settings and Secret portal families. PF17 already has a substantial unintegrated candidate; PF18–PF21 native backend families are absent. All PK1–PK6 implementation slices have integrated native or synthetic source, including the real private native UI journey. The key-store delivery gap is coherent installed activation/PAM, actual legacy acquisition, real frontend/sandbox and installed Settings qualification, and final dependency removal. Historical HANDOFF/TASK_LIST UI-journey summaries lag the actual source.

## Exact source boundaries

Direct git merge-base --is-ancestor checks returned exit0 for all source rows below except the PF17 candidate (exit1). These are source presence findings, not new runtime qualification.

| Outcome | Exact integrated/candidate evidence | Remaining bounded delivery |
| --- | --- | --- |
| PF17 Access/Notification/Email/Inhibit foundation | Candidate only: b8b621da074c288e1808e54ed7893c6a0e7b5c50, branch origin/feature/native-portal-foundation. Fork point bf5ebccb8b796fdd14ca98eff820d70fa0706dc6; target has15 other commits, candidate has1. Adds68 paths/3682 insertions. | Reconcile/review exact candidate; supervisor selected-session attachment; actual frontend/staged routing qualification. Inhibit lacks successful native producer scopes and end-session semantics. |
| PF18 FileChooser/AppChooser | No native backend source under src/services/portal at target or PF17 candidate; only existing fallback fixtures. | Separate chooser outcome over public file-manager/default-app models, native UI, request cancellation/parenting, frontend qualification. |
| PF19 Screenshot/ScreenCast | No native portal adaptors. PF16 application source32fd0fb94a482eb7f4845282f6e579867b899910 is integrated; src/apps/screenshot/platform/kwin_capture_port.cpp still uses org.kde.KWin.ScreenShot2 and matches owner to org.kde.KWin. | Portal request/session/consent + fork capture/PipeWire adapter; app existence does not implement portals. |
| PF20 RemoteDesktop/Clipboard/InputCapture | No native portal adaptors. KDE routing/drop-in remains. | Actual EIS sessions/consent, revocation, lock/input/capture boundaries, private native matrix and frontend switch. |
| PF21 Print/Account/DynamicLauncher/Usb/GlobalShortcuts and KDE retirement | No native portal adaptors or complete selector replacement. src/apps/settings/input/shortcuts_model.cpp is not a GlobalShortcuts backend. | Each family qualification, then coordinated fork interface/name/X-KDE caller rename and overlay removal. PF17 candidate supplies no restore-token store or portal Session lifecycle for capture/input. |
| PK1 storage |720a503ec96d217ee866ed2a4bd5cf64a306283e; src/services/keyring/src, public collection_store/secure_buffer and focused storage tests. | Final package/platform data qualification; implementation is present. |
| PK2 daemon/Secret Service |6e2832d2e6b58998219135450c741ae4a38d3339; daemon sessions/collections/aliases/prompts, unit/socket/descriptors, supervisor/keyring_session_lifetime.cpp. | Coherent installed single-owner activation/name switch and installed-client checks. |
| PK3 trusted PAM |f0d6edae17ea722a3d8604902cbe142e85f1ad69; src/services/keyring/pam module/helper/launcher/system-owner/template and pam tests. | Portage protected paths, actual stack ordering/system activation permission, reconcile prompt-only user service/socket versus trusted system owner, login/change/mismatch qualification. Native owner trust requires SO_PEERPIDFD/GetUnitByPIDFD/Yama; unsupported environments fall back to native prompt. |
| PK4 native Settings/prompt/policy |297d6fbfc45a751a0d3464f80d3cfc4d86040bab presentation; bf5ebccb8b796fdd14ca98eff820d70fa0706dc6 native journey and page-departure clipboard fix. src/apps/settings/keyring, services/keyring/prompt, tests/services/keyring/journey. | Integrated native journey uses production route in a test Window and production prompt sources in a test input binary; full installed Settings/prompt relocation, physical clipboard/outputs and real PAM remain. No need to reimplement journey. |
| PK5 Secret portal |326e4f80bece3a7bc135532d78b4d8e35326d7e8 integration of native493968e7 and exact legacy3736799e. src/services/secret_portal and daemon/portal_methods.cpp. | Actual Flatpak/installed frontend and live switch qualification; Secret selector already qindaqt in source. |
| PK6 complete legacy import |a55e542da50fde6dcabd9a3a190473945fe7ffe3 integration of939f33a9; src/services/keyring/import, app/import_main.cpp, collection_import transaction and test_legacy_import.py. | Run actual source-owner/PID-bound acquisition before name switch while legacy daemons remain installed; preserve original stores; then final packaging drop gnome-keyring/kwallet-pam. Synthetic complete-store and exact opaque64 implementations are present. |

## Routing and authority today

Source data/qindaqt.portal advertises exactly Settings;Secret. data/qindaqt-portals.conf has default=none; Settings/Secret=qindaqt; Access/AppChooser/FileChooser/Email/Inhibit/Notification/Print/Screenshot/ScreenCast/RemoteDesktop=kde;gtk;lxqt; GlobalShortcuts/InputCapture/Clipboard/Usb/Account/DynamicLauncher=kde; Wallpaper/Background=none. OpenURI is frontend-owned. The KDE process-local XDG_CURRENT_DESKTOP=KDE drop-in remains installed input.

PF17 candidate deliberately leaves the same declaration/selector. Its resident main starts PortalFoundationComposition and exports org.qindaqt.Portal1 at /org/qindaqt/Portal1. PortalSessionBinding retains the first same-UID session caller (including failed attachment) and requires public CompositorAttachment. Access/Email need attached live display plus authenticated Unlocked native state. Neither target nor candidate contains supervisor Portal1.AttachSessionWithDisplay consumption. Current supervisor references portals only for refresh/replaced activation. Therefore merging the candidate alone leaves these native families unreachable via routing and their positive privacy-gated paths unavailable in a normal selected session.

Inhibit source only accepts flags8 and requires native scopes mask7 (AutomaticLock, DisplayOff, IdleSuspend). Current PowerServiceObject default-constructs IdleInhibitorRegistry with empty consumedScopes and never calls setConsumedScopes in production. Native producer therefore advertises0 and refuses acquisition. Candidate CreateMonitor returns2 and QueryEndResponse errors; logout/user-switch/suspend flag combinations refuse. Retaining KDE Inhibit until real Power consumers qualify is required; changing its selector now would remove available behavior.

Secret uses actual frontend-owner app_id, fixed login collection, native state/prompt/secret receipts and owned writable FD; fresh-native32 and imported legacy-opaque64 records remain distinct. PK4 metadata receipts are already integrated through7973a70b270e7391c01da1a8933584ed4e87d45b. Native clipboard/prompt journey covers create/unlock/reveal/copy/rekey/delete, page departure, owner departure and actual native Locked with collection preference false.

## Qinda source and packaging observation

Read-only ssh -F /home/cabewse/.ssh/config qinda verifies bare hub main and ~/work_SPaC3/container-wm HEAD both equal audited target; its portal branch equals b8b621da. QindaGentoo checkout is d2fe9adf6469a396f6417c1864b8a01fd31f2851. Current source recipe gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20260928-r3.ebuild still depends on gnome-keyring and xdg-desktop-portal-kde; profiles/qindaqt/systemd/packages retains KDE portal. Read-only package directories show qinda desktop pre20260927-r7, gnome-keyring48.0-r1 and KDE portal6.6.6. No installed native key-store completion follows from repository source presence. No user data was opened.

## First safe bounded implementation recommendation

Assign one PF17A outcome: qualify native Access, Notification and Email through the actual private frontend and selected supervisor lifetime, based on reviewed/reconciled b8b621da atop the current manager base. Implement only missing Portal1 session attachment/lifetime and real-frontend/staged-positive/negative coverage, repairing resulting failures. After each family passes, update its explicit metadata/selector row; keep Inhibit and all later families at existing fallback. Stop after exact candidate + independent review + manager combined gates. This is a concrete native frontend delivery boundary, not completion of M5.

Requested ownership if assigned: src/services/portal; tests/services/portal; candidate prerequisite src/platform/foreign_parent and src/services/application_uri; narrowly named new portal lifetime helper in src/session_supervisor and focused supervisor tests; candidate Launcher URI grammar hunk; portal docs/reference/testing pages/ADR0318; minimal coordinated src/CMakeLists.txt/session build additions/mkdocs. Manager must coordinate supervisor paths with lock/power owners. Do not replace b8b foundation with a second implementation. A new ADR may be needed for durable supervisor lifetime choice beyond0318.

Acceptance must include actual real xdg-desktop-portal calls reaching the native resident, mapped Access grant/deny/Close, valid/lost parent, literal Email URI dispatched by synthetic configured handler, native Notification action/removal, frontend/portal/compositor/supervisor owner loss, restart attachment and no successful reply after privacy revocation. Repeat source/staged selector poison controls and preserve Settings/Secret. New tests should fail when the native selector/attachment is removed. Real Inhibit producer zero-scope refusal remains an explicit regression.

## Executable gates and resource boundary

Use a separate build root, existing exact fork production stage6ab6c01ede8143a7ddb477d6f0040e9b2f3753e4 or manager-pinned newer stage, host-uinput OFF, and j2 on laptop. Manager assigns stage path/configuration and native runtime slot first; no copied mutable build root. Candidate target names are actual CMake registrations.

```sh
cmake --preset dev
cmake --build build/dev --parallel 2 --target xdg-desktop-portal-qindaqt qindaqt-session qindaqt_portal_access_tests qindaqt_portal_notification_tests qindaqt_portal_email_tests qindaqt_portal_inhibit_tests qindaqt_portal_native_consent_tests qindaqt_portal_consent_input qindaqt_portal_foreign_exporter qindaqt_portal_mail_draft
QT_FATAL_WARNINGS=1 ctest --test-dir build/dev -R '^qindaqt\.portal-(access|notifications|email|inhibit)$' --output-on-failure --no-tests=error
# After manager's private native-runtime slot:
QT_FATAL_WARNINGS=1 ctest --test-dir build/dev -R '^qindaqt\.portal-native-consent$' --output-on-failure --no-tests=error
ctest --test-dir build/dev -R '^qindaqt\.portal-' --output-on-failure --no-tests=error
ctest --test-dir build/dev -R '^(secret_portal_|keyring_(collection_import|legacy_plan|legacy_import|pam_integration|pam_owner_policy|native_prompts|resident_policy|native_ui_journey)$)' --output-on-failure --no-tests=error
mkdocs build --strict
python3 tools/validate-docs
git diff --check
```

The complete portal selector includes the historical deliberate Settings wrong-signature warning row; do not globally force fatal warnings for that row. New native-family rows retain fatal warnings. Add the new frontend/supervisor checks to their actual CMake registrations so -R selects them. The PK native_ui_journey is serialized native work too; the private-bus rows also require the manager's runtime allocation when names/resources may collide. Required gates must report zero skips; a SKIP77 is unavailable evidence.

## Audit verification and stopping point

- git fetch origin: exit0.
- Read-only ancestry/source/metadata and qinda SSH inspections: successful; direct ancestry results above.
- python3 tools/validate-docs: exit0,475 Markdown documents/navigation.
- mkdocs build --strict --site-dir .cache/pf-portals-docs: exit0.
- git diff --check: exit0.
- Product build/CTest/native scenarios: deliberately not run under audit-only scope. Candidate commit's reported25/25 and native54-check evidence are historical claims, not independently rerun audit results.
- Changed paths: this handoff, claim/finding/help message siblings and ops/team/workers/pf-portals-sol-20261001.md only.
- No product weights, installed services, packages, source behavior, host credentials or legacy data changed.
- Requested next action: manager route b8b621da exact source review/reconciliation, then assign bounded PF17A ownership/build configuration/runtime slot. PK final delivery can be planned in parallel by packaging owner without touching real stores yet.

Read platform queue and relevant peer threads after handoff. Offer: implement the scoped Portal1 supervisor lifetime + actual frontend Access/Notification/Email qualification once explicitly assigned. Until then this worker is waiting; no unscoped implementation.
