# QindaLink vault-typing handoff

- Lane: vault-typing (vault part 2). Branch: link/vault-typing. Base d38076889, merged main 42edc40cb.
- ADR: 0440 (docs/wiki/adr/0440-qindalink-secret-typing-focus-relay.md).

## Delivered

- Protocol: "Desktop facts and secret typing" section (desktop.focus, vault.type, bounds, errors).
- Desktop: src/services/link_focus (FocusStore), src/services/link_vault (SecretPlan, VaultTypingFeature),
  Link1.Focus.Publish (shell-only via CallerRole::Shell, CallerPolicy second set, --trusted-shell-caller),
  TextTyper enqueueSymbols/batch guard/zeroing, InputFeature::setPortalBorrowed, FeatureHandler::extraNamespaces.
  Test-only service flags: --trusted-shell-caller, --test-portal, --test-open-gate.
- Shell: src/shell/runtime/linkfocusrelay.{h,cpp}; TaskListAppletComposition::windowFacts; wired in
  shellruntimeapplication_applets.cpp.
- Phone: VaultWireSteps, VaultTypeFlow, VaultTypeFeature, FeatureContext.request + Session reply routing,
  VaultBridge typeBegin/Confirm/Cancel/State, VaultTypeSheet.qml, preview FakeVault.

## Evidence (build/vt, base flags from the brief)

- ctest qindaqt.link-* (29 rows incl. link-vault 19 checks, link-vault-service 3, link-caller-coverage,
  link-caller-dbus, link-input, link-macros): all pass; the not-built link-applet rows are Not Run.
- qindaqt.shell-link-focus-relay (4), qindaqt.task-list-applet-composition-private-bus,
  qindaqt.shell-runtime-layout-adoption, qindaqt.shell-panel-access-threading: pass. qindaqt-shell links.
- phone/engine-tests/run.sh: all pass, VaultTypeTests 50 checks. phone/engine-tests/run-cross-vault.sh build/vt: 3 passed.
- phone/preview/run-smoke.sh: 0 failures, 0 QML warnings. phone/tools/build-apk.sh: built (not installed).
- mkdocs build --strict and tools/validate-docs pass.

## Caveats

- No live run: the real portal prompt and the phone screen need the user. Nothing was installed on the phone.
- A short race remains between the last focus check and delivery of a batch of at most 24 symbols.
- Portal calls trail replies, so tests settle before asserting.
- tools/check-source-shape: shellruntimeapplication.cpp (+1 line) and tasklistappletcomposition.cpp (+25) were already over thresholds.

## Requested next action

Review and merge; then a live run with the user (approve the remote-control prompt once, type into a scratch window).
