# PF18 immutable native FileChooser and AppChooser handoff

- Worker: pf-portal-choosers-sol-20261001, OpenAI Codex implementer.
- Exact candidate: **684ca904b3b997d293795abb413cb1e2f94aa666**.
- Exact base: 8c717c826e334674a0a39224361af07bf49fd9ac.
- Branch: worker/pf-portal-choosers-20261001, preserved on qinda hub.
- Source: laptop .cache/pf-portal-choosers-20261001 and qinda /home/cabewse/work_SPaC3/container-wm.worktrees/pf-portal-choosers-20261001 both clean at candidate before this record-only child. This handoff/worker-state/help child changes only this worker's ops records; product, tests and wiki remain exact candidate bytes.
- Resources: compiler and private-runtime released to root at11:39UTC; no active implementer build/test or reservation. Actual Portage MAKEOPTS -j24 -l24 unchanged.
- Requested next action: different-worker immutable verdict on this exact candidate, then manager integration and combined-tree gates. No installed cutover or full portal completion claimed.

## Delivered contract and changed paths

FileChooser version4 implements all OpenFile, SaveFile and SaveFiles methods; AppChooser version1 implements ChooseApplication and UpdateChoices. Version1 deliberately makes no version2 activation-token promise. Native selectors and metadata now advertise both completed families after real frontend/native qualification; other family routing assertions remain intact.

Separate bounded pure policy/wire types, frontend request adaptors, process lifetime port and ordinary Qt widget dialogs keep GUI/file-system presentation outside the resident service. Public ApplicationCatalog supplies installed canonical app IDs; no command parsing or launching. Canonical local fully encoded file URIs, filters, Boolean/combobox choices, app IDs, cancellation and overwrite decisions are revalidated at the publication boundary. The file chooser never writes/reserves chosen files. Existing file replacement asks the user with No as the default; SaveFiles generates bounded distinct suggested names.

Actual current frontend/requester/request-handle owners, public selected-session binding and read-only native lock admission fence publication. Close, requester/frontend/supervisor/parent loss, invalid updates, bounded child output and late replies fail closed. The helper gets an ordinary compositor FD and public foreign-parent import, never ambient display authority. Trusted package-relative helper construction retains the old six-argument foundation constructor as explicitly chooser-unavailable. No production Access, Email, Notification, Settings, Secret or InhibitPower1zero behavior changes.

Owned implementation paths are src/services/portal/choosers/**; new public app_chooser_adaptor.h, file_chooser_adaptor.h, chooser_types.h, chooser_ui.h and process_chooser.h; new corresponding chooser policy/wire/frame/adaptor/process files under src/services/portal/src. Focused tests live under tests/services/portal/choosers/**.

Approved additive coordination paths: src/services/portal/{CMakeLists.txt,foundation/CMakeLists.txt,src/foundation_composition.cpp,include/qindaqt/services/portal/foundation_composition.h,app/main.cpp,data/qindaqt.portal,data/qindaqt-portals.conf}; tests/services/portal/{CMakeLists.txt,foundation/CMakeLists.txt,check_boundary.cmake,run_staged_package.cmake,tst_portal_frontend_integration.cpp,tst_portal_frontend_routing.cpp,portal_chooser_routing_probe.h}. Routing fixture changes prove actual native Response2 with primary frontend typed uris=[] and KDE counter0 without selected attachment, retaining still-KDE family coverage. Foundation test dependency additions ensure actual consent-input/mail/URI-relay artifacts exist in isolated focused builds.

Documentation paths: docs/wiki/reference/portal-choosers.md, docs/wiki/adr/0322-native-portal-choosers.md and ADR index; architecture portal-service.md, portal-foundation.md, module-boundaries.md; reference portal-settings-backend-v1.md; mkdocs.yml. Primary installed xdg-desktop-portal1.20.4 XML and upstream source were inspected directly. ADR0322 records the separate ordinary helper boundary.

## Exact executable acceptance

Fresh qinda Ninja Debug, sharedON, pluginOFF, productionShellON build/choosers uses the qualified stage690 compositor at /home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/pf-knighttime-gate/stage/usr/bin/qindaqt-kwin. Production authorization is OFF in this qualified fixture. Builds install nothing on the host; staged packaging is disposable test-local output.

Production backend/helper and chooser policy/request/native targets built successfully with strict production warnings. Adjacent portal/Secret targets built successfully. The final exact-candidate dependency repair rebuild returned **exit0**:

```sh
ninja -C build/choosers -j24 -l24 \
 qindaqt_portal_frontend_integration_tests \
 qindaqt_portal_frontend_routing_tests \
 qindaqt_portal_native_chooser_tests \
 qindaqt_portal_native_consent_tests \
 qindaqt_portal_native_frontend_tests
```

Final exact-candidate command, run from the qinda worktree, returned **exit0,24/24 CTests,0 failed/skipped,31.64s** (21 portal plus3 Secret):

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
 DBUS_SYSTEM_BUS_ADDRESS=unix:path=/home/cabewse/work_SPaC3/container-wm.worktrees/pf-portal-choosers-20261001/build/choosers/unavailable-system-bus \
 QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=0 \
 ctest --test-dir build/choosers --output-on-failure \
 -R '^qindaqt.portal-|^secret_portal_(policy|fd_wire|broker)$' -j1
```

Native runners enforce QT_FATAL_WARNINGS=1 independently; Secret rows also retain their fatal-warning environment. Only deliberate existing invalid-method service rows need normal warnings. Staged package row passes6.68s, native consent/frontend rows pass, source boundary/poison rows pass, real frontend selection/routing native-denial rows pass.

The actual native chooser journey passes9 QtTest cases,0 failed/skipped: open one/multiple/directory, save suggested path and overwrite No/Yes, SaveFiles, offered MIME/glob filters and Boolean/combobox choice values; installed offered AppChooser IDs, actual updates/cancel; registered caller and foreign-parent validity/loss; requester/frontend/supervisor/Close withdrawal with late-reply fencing. It maps real ordinary dialogs against the real frontend/compositor and drives real Qt keyboard/mouse events. The separate input executable links unchanged production dialog sources and does not inject a successful result or add production auto-selection.

Independent reviewer pf_sleep_review reports the same exact24-row command **24/24 PASS,0 failed/skipped,31.77s**. That report does not substitute for the reviewer's separately authored verdict.

Other completed gates: python3 tools/validate-docs exit0,482 documents plus navigation; mkdocs build --strict -d .cache/chooser-docs exit0 (documentation bytes identical at final candidate); focused python3 tools/check-source-shape --root src/services/portal --json exit0,71files/0issues; boundary plus existing mutation poisons exit0; runner syntax and git diff --check exit0. Whole-tree shape checker had168 issues across5431files and is not claimed as passing or baseline-equivalent. No new hand-written production chooser file approaches500 nonblank lines.

## Preserved failures and causal repairs

Initial strict compile caught misleading indentation in chooser_frames.cpp; repaired before production build pass. Initial native runs caught premature attachment before actual compositor bus-owner readiness, a checkbox hit-area input error, and absent private GIO MIME associations. Actual owner wait, keyboard Space and private mimeapps.list repaired the test inputs; failure logs retained.

The first broad9aa run passed18/24 and failed6 rows in205.86s. A blanket fatal-warning setting aborted an existing intentional invalid-method row. Three new denial assertions incorrectly expected an empty frontend dictionary; primary1.20.4 actually adds typed uris=[], now asserted exactly with Response2 and KDE0. An installed Power1 was activated on the private default bus in that first run: this was a harness-isolation defect, not an allowed host-service interaction or final caveat. The repaired tests reserve org.qindaqt.Power1 on a test-owned connection before backend startup and explicitly set a nonexistent system bus; custom native runners have no host activation directories. Final reruns use these repaired inputs.

The two unchanged native consent/frontend rows failed to map because their focused build omitted qindaqt_portal_consent_input. A single bounded stderr capture proved ENOENT, not an Access admission or platform-theme regression. The minimal approved CMake dependencies now build consent input plus the required mail/URI-relay artifacts with those focused targets. No production Access policy relaxation or fallback was needed. The preserved direct stderr is /home/cabewse/.cache/pf-portal-choosers-20261001-debug/consent-stderr.log.

Evidence logs are immutable sibling files on qinda under /home/cabewse/work_SPaC3/container-wm.worktrees/:

- pf-portal-choosers-20261001-complete-fixtures-build.log — final candidate successful five-target rebuild.
- pf-portal-choosers-20261001-complete-fixtures-tests.log — final candidate24/24.
- pf-portal-choosers-20261001-final-tests.log — preserved first broad9aa failure.
- pf-portal-choosers-20261001-native-repair2.log and -native-repair3.log — failed native input evidence and first9-case passing journey.
- pf-portal-choosers-20261001-build.log and -build-repair1.log — first strict compile failure and repaired production build.

Earlier worker replies used Ninja progress denominators as action totals. Those denominators can shrink; the trustworthy build claims here are exit0 and named target scopes, not executed-action counts.

## Remaining boundary

No installation, actual host file selection, live desktop service replacement, physical display claim, sandbox document export/registration, or complete Flatpak/GTK delivery is covered. Canonical URI decisions and displayed native private dialogs are qualified; installed/sandbox delivery and manager combined production/plugin gates remain separate. The source is ready for exact independent verdict and integration, not a claim that every portal family is complete.
