# Lane K (Opus): QindaQt's own key store, slices PK1–PK3

- Worker name `claude-keyring`; speech name "key store". The voice seat is given in the launch prompt.
- Worktree: `cd ~/work_SPaC3/container-wm && git fetch hub && git worktree add ~/work_SPaC3/container-wm.worktrees/keyring -b feature/qindaqt-keyring hub/main`.
  Push to `hub`.
- ADR numbers: take the next free ones after the other lanes' reservations (0289 screenshot, 0290 polkit). Check `docs/wiki/adr/` on `hub/main` and on those branches.
- **Spec:** `docs/plans/2026-09-28-plasma-free-qindaqt.md` §3.8 on hub branch `plan/plasma-free` (7854ad3a). Read the "Guiding principle" and "Owner decisions" too. Owner: "it needs its own key store". It must be a native QindaQt component, not a port.

**Deliver PK1, PK2 and PK3, one reviewable commit or more per slice:**
- the storage core;
- the Secret Service daemon with `org.qindaqt.Keyring1`, the systemd user unit and socket, supervisor wiring, and the single-owner rule;
- the PAM module (login unlock, password change, mismatch recovery).

For prompts, add a minimal QindaTK prompt process that PK4 will extend. A scripted prompt is enough for the tests.

**Security bar:**
- Use OpenSSL 3 EVP only (Argon2id, AES-256-GCM). Check that the installed OpenSSL has Argon2id; if not, stop and report.
- No custom crypto primitives. Constant-time comparisons. Locked and zeroed secret memory.
- No secrets in logs, core dumps (`PR_SET_DUMPABLE` 0 for the daemon) or D-Bus error texts.
- Peer-credential checks on the control socket.
- Atomic file replace with fsync.

**Tests** (targeted only):
- The unit, protocol (private bus, `secret-tool` and `secretstorage`) and `pam_wrapper` tests that §3.8 lists.
- The DH session against libsecret.

**Never:**
- touch the live session;
- claim the real `org.freedesktop.secrets`;
- read the owner's existing keyrings or wallets;
- run the import against real data (that is PK6, manager-run, later).

No packaging: the manager packages once at the end. Build only your targets on qinda with `-j8 -l24`.

**Report:**
- commits, files, and tests with counts and exit status;
- the recommended packaging and PAM lines;
- the threat-model notes for the owner.

Keep token use low.
