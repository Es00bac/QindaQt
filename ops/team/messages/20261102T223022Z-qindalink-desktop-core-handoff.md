# QindaLink desktop-core handoff

- Lane: desktop-core (M1b), branch `link/desktop-core`, base `f6c27bc90`.
- Candidate commit: `e1f7c30a265acee82fc6289cb2f18f19f15040e8` (the branch head adds only this handoff file).

## Changed paths

- `src/services/link_protocol/`, `link_transport/`, `link_adb/`, `link_service/`, `link_client/` (new)
- `tests/link/` (new tests and `support/`; `vectors/` untouched)
- `docs/wiki/reference/link1-v1.md` (new), `docs/wiki/architecture/qindalink.md` (maturity line, trust-store row), `mkdocs.yml` (nav)
- Additive lines in `src/CMakeLists.txt` (services are registered there) and `tests/CMakeLists.txt` (`add_subdirectory(link)`)

## Evidence

Configure: `cmake -S . -B build/desktop-core -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBEXECDIR=libexec -DBUILD_TESTING=ON -DQINDAQT_BUILD_SHELL=OFF -DQINDAQT_BUILD_KWIN_PLUGIN=OFF`
(the install prefix is needed here because the portal capture CMake check compares against `/usr/libexec`).
`ctest -R qindaqt.link-` exit 0, 7/7 passed: vectors (40 frame cases, 3 pairing cases), identity (4), pairing (10), session (8), adb (4), client (2), service-smoke (1, real binary on a `dbus-run-session` bus).
`tools/validate-docs` ok; `mkdocs build --strict` ok; `tools/check-source-shape` reports no link file (largest 333 lines).

## Caveats

- `Link1` has one method beyond the brief: `ListenPort() -> q`, used by the smoke test (service runs with `--port 0`). Documented in link1-v1.md.
- Service switches `--data-dir`, `--port`, `--no-adb`, `--no-notifications` exist for test isolation.
- The notification presenter (`QtNotificationPresenter`) is implemented but only exercised by the interface; no test posts to a real notification server.
- LAN listener logic (replacement, denied-without-window) is tested with a second ephemeral listener; the service never opens a LAN listener.
- The systemd user unit relaxes ProtectHome/ProtectSystem (identity dir may not exist yet); noted in the unit.
- No ADR added: no new dependency or boundary beyond ADR-0433/0434. OpenSSL 3.2 libcrypto is already a tree dependency.
- Not run: broad suites, packaging/Portage install of the new component (`QindaQtLink`), real-phone test.

## Requested next action

Integrate; wire `QindaQtLink` into packaging when the app/applet lanes land.

## Repair round (review P1/P2)

- Merged main 24e8ce047; repair commit `4dcca0afc09debcdedda046900b01a6976fa8387` (branch head adds only this handoff update).
- P1: unit (no PrivateDevices, AF_NETLINK, KillMode=process, AGENT-NOTE); unauthenticated bounds (hello 10 s, UNPAIRED 120 s not extended by ping, max 8, one pairing at a time) in `qindaqt.link-bounds`; READY refused with `bye{error}` when the pin cannot be written (test makes the atomic rename fail), trust store rolls back.
- P2: type regex anchoring, integer-token ids, nesting depth 64, strict hex np, notification sanitising (`qindaqt.link-presenter`), identity fail-closed/atomic/0700-at-creation (`qindaqt.link-identity`), decoder offset compaction.
- Evidence: `ctest -R qindaqt.link-` 9/9 passed; validate-docs and `mkdocs build --strict` ok.
- Known gap: ConfirmPairing caller restriction (documented in reference/link1-v1.md).
