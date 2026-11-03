# QindaLink caller-policy handoff

- Branch `link/caller-policy`, base `6f58e0df1`, candidate `2cdd32caf69132edffdb552d1fcdfc870e435e65` (head adds only this file).
- ADR-0438. Code: `link_transport/caller_authority.h` (interface), `link_service/{caller_policy,send_guard}`, `link_messages/send_gate.h`, adaptor changes in link_messages and link_notifications, Link1Service (PendingPairings, guards), Hub (caps, pendingPairings), service `--trusted-caller`.
- Tests: `ctest -R qindaqt.link-` in build/caller: 18/18 passed, including new `link-caller-policy` and `link-caller-dbus` (untrusted gdbus denied on all 12 protected members, open members work, trusted test binary succeeds, caps, PendingPairings, 5 accepted + 1 RateLimited, 5 recorded notices). `run-cross-impl.sh` and `run-cross-notify.sh` OK with `--trusted-caller` gdbus. validate-docs and mkdocs --strict ok.
- Not built here (QINDAQT_BUILD_SHELL=OFF): shell link_applet tests; the applet uses only open members plus ConfirmPairing/Ring/Unpair as qindaqt-shell.
- Caveats: dev builds of qindaqt-link/shell outside bindir need `--trusted-caller`; the pairing notification's Accept/Reject buttons bypass the policy by design (documented); the send notice is skipped when no presenter is configured but the rate limit still applies; CurrentChanged consumers that read the item text must call Current().
- Next: when the qindaqt-link app lands, confirm its installed name is `qindaqt-link`.
