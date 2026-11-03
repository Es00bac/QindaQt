# QindaLink caller-policy-2 handoff

- Branch `link/caller-policy-2` from bb2e7c777, candidate `306791e22c88300bebdcccf0427524747450ff2a` (head adds only this file).
- P1.1 inode identity + mount namespace (fake-inspector tests: bind-mount string spoof, other namespace, package upgrade). P1.2 `qindaqt.link-caller-coverage` (16 methods seen, 12 protected denied; allowlist Devices, PendingPairings, ListenPort, Revision). P1.3 --trusted-caller help + stderr warning. ADR-0438 gains "What this does not stop".
- P2: SendGuard refuses unknown devices (`not-paired`), bucket count bounded. pidfd not implemented (optional).
- Evidence: build/caller2, all link test targets built; ctest -R qindaqt.link- 21/21; both cross scripts OK; validate-docs, mkdocs --strict ok.
- Caveat: a legitimate client in a different mount namespace (sandbox) is now denied by design.
