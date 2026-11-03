# QindaLink desktop-app handoff

- Branch `link/desktop-app`; the candidate is the commit that adds this file (its sha is the lane's final line).
- Merged main (ADR-0438 caller policy) before finishing.

## Changed paths
- New app: `src/apps/link/**` (core seam + fake, view models, AppShell actions, QML), `src/CMakeLists.txt` (`QINDAQT_BUILD_LINK_APP`).
- Service: `src/services/link_service/{phone_setup,setup_adaptor}`, `link1_service.cpp` (one registration line), `org.qindaqt.Link1.xml`. All three `Link1.Setup` members use the shared `CallerAuthority` (Denied for untrusted; null authority denies).
- Mirror: `--list-json` (`mirror_options`, `evidence_runner`, `main.cpp`).
- Tests: `tests/apps/link/*`, `tests/link/tst_link_setup.cpp`, `tst_link_caller_dbus.cpp` (Setup members added to the protected list, plus a trusted-caller check).
- Docs: `docs/wiki/apps/qindalink.md` (+ mkdocs), architecture row/maturity, `link1-v1.md` Setup section, mirror reference.

## Tests (QT_QPA_PLATFORM=offscreen, build/app)
- qindaqt_link_app_conversation_test 13/13, _devices 12/12, _phone_apps 11/11, _options 9/9, qindaqt_link_setup_test 6/6 (exit 0).
- qindaqt_link_caller_dbus_test under dbus-run-session 3/3.
- Renders: `qindaqt-link --fake <scenario> --page <p> --grab` for every page in populated, empty, unavailable and error states, all viewed; reply window rendered.

## Caveats
- Not run: the phone, real service, real session bus or menu. Global menu export was not exercised on a private bus (only that AppShell accepts the catalog, tested); needs a registrar.
- `PendingPairings` is called and tolerated if absent; its list path is tested with the fake only.
- Input page reports "unavailable" via the `desktopInputBuilt` constant; flip when the PM confirms the input feature is live.
- Packaging (`org.qindaqt.Link.desktop`, icon) installs to bindir `qindaqt-link`; no ebuild changes made.
- Not rebuilt: full-tree/other suites.

## Next action
PM: integrate; wire the app into the desktop package and run one USB smoke with the user's approval.
