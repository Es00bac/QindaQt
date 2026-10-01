# PF17A native frontend qualified candidate handoff

- Worker: pf-portals-sol-20261001
- Time: 2026-10-01T03:10:50-06:00
- Product source boundary: 9c3d0489 (full SHA resolves with git rev-parse9c3d0489); this record is board-only over that exact source.
- Branch: worker/pf-portals-20261001, pushed to qinda hub.
- Requested next action: independent exact candidate review; manager integration/combined gates after acceptance. No installed cutover.

## Outcome

Reconciles preserved PF17 foundation b8b621da as91bf7826 atop ab764094 with only additive testing-harness/mkdocs conflict resolution. Native Access, Notification and Email are selected/advertised; Settings and Secret remain native, every other family retains its existing fallback/closed row. Inhibit remains KDE because current Power producer consumes no native complete scopes. No PF18–PF21 stubs or progress claims.

PortalSessionLifetime is a separate supervisor collaborator: optional native helper, prefix-relative bin/libexec discovery, retained dedicated selected-session bus, canonical ordinary display, exact unique owner/same UID, bounded asynchronous attachment and owner-replacement reattachment. Queued identical-owner advertisements cannot replay already selected attachment. Stop disconnects selected-session lifetime before child teardown. Backend public CompositorAttachment/native Unlocked receipts remain authoritative; attach acknowledgements supply no new privacy authority. --no-portal explicitly isolates private sessions.

The real xdg-desktop-portal1.20.4 reaches Access through Camera.AccessCamera, Email.ComposeEmail and Notification. Production ResidentPortalService/PortalFoundationComposition, native notification host, mapped production consent source/test input and synthetic configured mail handler provide executable dispatch/results. Public Close, frontend owner loss, supervisor loss and selector withdrawal fail closed. The session fixture exercises a distinct replacement backend unique owner with the same retained supervisor caller. Staged package repeats frontend positive/withdrawal controls with installed metadata and URI relay; the consent input binary remains an explicit production-source test driver.

## Changed paths

- The exact68 prerequisite paths from b8b621da are preserved (portal modular policies/adaptors/consent, foreign_parent, application_uri, narrow Launcher URI grammar/test, primary wiki/ADR0318/module boundaries/testing/nav/build additions).
- src/session_supervisor/src/portal_session_lifetime.h/.cpp (new); src/session_supervisor/src/session_process_supervisor.cpp; include/qindaqt/session_supervisor/session_process_supervisor.h; app/main.cpp; CMakeLists.txt.
- tests/session_supervisor/tst_portal_session_lifetime.cpp (new), CMakeLists.txt.
- src/services/portal/data/qindaqt.portal/qindaqt-portals.conf; tests/services/portal/check_boundary.cmake/CMakeLists.txt/run_staged_package.cmake; foundation/tst_native_frontend.cpp/run_native_frontend.py/CMakeLists.txt.
- docs/wiki/architecture/portal-foundation.md/portal-service.md, reference/portal-settings-backend-v1.md, adr/0318-native-portal-foundation.md, development/testing-harness.md. The obsolete GNOME Secret paragraph now matches already integrated native source.
- Self-owned worker/messages only; no manager task/features/HANDOFF, power/idle/DPMS source, compositor manifest, overlay or installed files edited.

## Verification

Qinda source/build: ~/work_SPaC3/container-wm.worktrees/pf-portals-20261001, build/dev. No mutable shared build copied. Read ~/AGENTS.md and portageq envvar MAKEOPTS directly: -j24 -l24; direct Ninja used those exact configured limits under manager exclusive slots. Compiler/private runtime slots released.

Configure (exit0): Ninja Debug, BUILD_TESTING=ON, BUILD_SHARED_LIBS=ON, QINDAQT_BUILD_KWIN_PLUGIN=OFF, host-uinput OFF, strict warnings ON, C/C++ ccache. CMAKE_PREFIX_PATH and QINDAQT_KWIN_WAYLAND explicitly use native-night-light-integrated-gates/stage/usr. Manager identifies its source5f6fdf11; current consumer pin remains6ab6c01. This proves ordinary fork protocol/runtime use, not an exact consumer compositor ABI gate. Existing QindaTK keyring test-plugin configure warning remains unrelated.

Build commands use ninja -C build/dev -j24 -l24 with production xdg-desktop-portal-qindaqt/qindaqt-session, all portal policy/source/service/lifecycle/frontend/native fixtures, qindaqt_portal_session_lifetime_tests, production consent/URI helpers, qindaqt_session_supervisor_tests and qindaqt_launcher_execution_tests. Initial503-action build found one incomplete public fixture header; repaired7-action build passes. Production helper25-action build passes; supervisor ownership/install-path repair14-action build passes; final frontend/adjacent49-action build passes, all exit0. Mistyped target names were rejected before compilation and corrected; no result is claimed for those invocations.

Executed:

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir build/dev -R '^qindaqt\.(portal-(access|notifications|email|inhibit)|session-portal-lifetime)$' --output-on-failure --no-tests=error
ctest --test-dir build/dev -R '^qindaqt\.(session-portal-lifetime|portal-native-(consent|frontend))$' --output-on-failure --no-tests=error
ctest --test-dir build/dev -R '^qindaqt\.(portal-.*|session-portal-lifetime|session-supervisor|launcher-execution)$' --output-on-failure --no-tests=error
ctest --test-dir build/dev -R '^qindaqt\.portal-staged-package$' --output-on-failure --no-tests=error
python3 tools/validate-docs
mkdocs build --strict --site-dir .cache/pf-portals-docs
git diff --check
```

- Four native-family rows pass. First supervisor fixture exposed duplicate queued owner attachment; exact repair makes the distinct-owner replacement/canonical/disconnect row pass.
- Private native consent passes. First actual frontend row passes8 checks/fails Email; fixture wrote defaults outside the production factory's documented lookup. Correcting to XDG_DATA_HOME/applications/mimeapps.list makes actual native frontend pass. No product shortcut bypassed the public store.
- Combined affected selector:20/21 CTests pass, exit8 solely because new staged proof ran after legacy script removed its prefix. This includes passed source native frontend, actual native consent, complete70-second supervisor suite, new lifetime row, Launcher execution, existing Settings source/signatures/lifecycle/selection/toolkit/routing and metadata negative controls.
- Script-only stage ordering repair9c3d0489: exact failed row repeats1/1 pass, exit0,6.67s; includes native positive/withdrawal controls. All21 selected rows are qualified across original20 passes plus repaired row, zero skips. Do not describe this as a clean single21/21 run or full repository suite.
- Strict MkDocs,477-document links/navigation and diff check pass exit0.
- Decomposition review: existing supervisor557 nonblank lines now delegates the cohesive portal responsibility to an independent helper; new production files remain below500. No central policy/persistence was added.

Preserved logs in qinda worktree build/dev: configure.log, build.log, build-repair.log, build-helpers.log, focused.log, build-lifetime-repair.log, native.log, configure-repair.log, build-frontend-repair.log, qualified.log, staged-repair.log. Historical failures remain preserved with causal repairs rather than rewritten.

## Bounded caveats/stopping point

Source qualified; independent review and manager integrated-tree checks remain. Stage uses newer fork runtime with plugin OFF; no ABI, physical/installed desktop, sandbox Flatpak, real mail, PAM, live keyring migration, host bus/service, package deployment or KDE dependency removal is claimed. Consent test input is synthetic visible Qt input in production sources; installed physical consent remains a delivery gate. PK source audit remains in a86b6046675664a55b1cb810c8f6a4f4cbd7939f; its native UI journey and importer already exist. Full Inhibit semantics and PF18–PF21 remain successive assignments.

Read platform queue/relevant peer state after handoff. Concrete help offered: independent exact review of runtime891092f66029fdffa50258e98b6d52d3ed61007b when the manager's formal next assignment arrives. Status waiting; no unscoped implementation or new runtime claimed.
