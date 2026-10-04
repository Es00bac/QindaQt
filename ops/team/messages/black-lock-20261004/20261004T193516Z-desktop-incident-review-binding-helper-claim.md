# Temporary native keyring binding diagnostic claim and trust finding

- Timestamp: 2026-10-04T19:35:16Z
- Owner: `/root/desktop_incident_review`, temporary cache-only client; permanent source owner `/root/native_permission_repair`.
- Trust source: currenteeed1f6cac509990f64fd1f692a796f370206c55; native API/display attachment/CompositorNames sources unchanged from deployedab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b (exact qinda hub diff exits0).

Source permits same-UID temporary API use: SecretService::handleMessage checks caller UID; AttachSessionWithDisplay accepts empty/same first unique session owner; SessionDisplayBinding selects that owner and public CompositorAttachment proves canonical display, actual same-UID compositor UNIX peer/PIDFD and bus owner/PID lifetime. It does not attest a supervisor executable. Native peer validation stays server-owned; helper will only invoke AttachSessionWithDisplay/GetPolicyState plus bus process/name metadata, no Wayland protocol, secret/session/unlock/mutation/shutdown call.

Material consequence: accepted caller ownership is keyring daemon lifetime ownership. SecretService::ownerLost exits when that client unique owner disconnects. Closing helper on signals therefore ends/wipes the current attached daemon, not merely display detachment. Reported to root; do not add arbitrary time expiry while compositor lives. Helper pins supplied initial compositor owner/PID, canonical basename, equal native/standard keyring owner/PID/UID and owns a private connection. Keyring replacement stops/review, never reattaches silently.

Root alone will review helper and execute live metadata/API adoption. No live AttachSession from worker, installation/startup software or source/global config edits. All tests/private fixtures/heavy work stay on qinda under current explicit owner instruction; laptop game development retains its resources. Lightweight installed Python module/source reads only performed locally. Helper/test source/cache will be preserved on qinda; board remains current.
