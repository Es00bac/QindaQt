# gap-keyring handoff — 2026-09-11

- **Candidate commit:** `7717c996d495` ("Bootstrap the provider test through the PAM-equivalent login flow")
- **Base commit:** `7dad9e78f117d7fb492d381d631d7cec637ce1e5`
- Lane `gap-keyring`, branch `gap/keyring`, worktree `/home/cabewse/work_SPaC3/container-wm-workers/gap-keyring`.

## Outcome delivered

Browsers, mail clients, Electron applications, and Flatpak applications keep
passwords in the unlocked gnome-keyring `login` collection in every QindaQt
session, never in plaintext-equivalent storage. The provider choice is
recorded (ADR-0135), the login unlock contract is documented, applications
that choose a password store from the desktop name are pointed at the keyring
with documented flags and a packaging request, and a private-bus ctest row
proves the provider contract. All four new ctest rows and all eight existing
secret/session-environment rows pass.

## Changed paths (`git diff --name-only 7dad9e78..7717c996`)

```
docs/wiki/adr/0135-gnome-keyring-secret-service.md   (new)
docs/wiki/adr/index.md                                (ADR-0135 row)
docs/wiki/architecture/secret-service.md              (new)
docs/wiki/handbook/privacy.md                         (Passwords and keys section)
mkdocs.yml                                            (Secret Service nav + ADR-0135 nav)
tests/CMakeLists.txt                                  (add_subdirectory(services/secret_service))
tests/services/secret_service/**                      (new: scenario, parsing test, fixtures)
tools/keyring-check/qindaqt-keyring-check             (new)
```

Commits on the branch, in order: `f8382a26` (ADR-0135), `ddca61cc` (docs),
`0d12e95c` (contract test + tool), `7717c996` (bootstrap fixes).

## What the deliverables pin down

- **ADR-0135** (`docs/wiki/adr/0135-gnome-keyring-secret-service.md`):
  gnome-keyring's secrets component is the only Secret Service provider,
  reached through D-Bus activation; KWallet and ksecretd stay unused (no
  wallet to adopt, PAM unlock never fires, activation-file ownership); the
  PAM lines a distribution must ship (`-auth optional`, `password …
  use_authtok`, `session … auto_start` for `pam_gnome_keyring.so`); locked
  collections prompt through `gcr-prompter` on Wayland and must never hang
  clients; gcr-ssh-agent exists on this machine
  (`/usr/libexec/gcr-ssh-agent`, user socket/service units) and QindaQt
  deliberately sets no `SSH_AUTH_SOCK` and starts no agent.
- **Password-store research (in the ADR):** sync-era Chromium os_crypt
  (Electron 41 / Chrome 146) selects plaintext-equivalent `basic` on an
  unrecognised desktop name; async-era os_crypt (Chromium 152) selects the
  Secret Service; `--password-store` overrides both; Firefox 155 and
  Thunderbird 155 use their own NSS storage and are unaffected. Verified from
  installed binaries (`strings` on `/opt/{chatgpt,claude-desktop,ZCode,sloom-studio}`
  and `/opt/{firefox,thunderbird}`), upstream sources at tag 138.0.7204.49 and
  main, and Electron's safeStorage documentation. ChatGPT (Chromium 152) is
  already correct; claude-desktop, ZCode, and sloom-studio need launch flags.
- **Provider contract rows** (`tests/services/secret_service/`): the scenario
  runs inside `dbus-run-session` with a generated bus config whose only
  activation directory is empty (the system
  `org.freedesktop.secrets.service` must stay invisible), a disposable
  XDG tree, and a per-case `XDG_RUNTIME_DIR`. It reproduces the PAM login
  flow — a `--login` daemon holding the test password plus a `--start`
  daemon that sends the control-socket initialize handshake — then asserts:
  `org.freedesktop.secrets` owned, default alias resolves to an unlocked
  collection, a secret round-trips through `secret-tool`, locking the
  collection makes lookups return nothing without prompting, a wrong
  password leaves the collection locked, and a client without a provider
  fails closed inside a bounded time. Rows skip with exit 77 only when
  `gnome-keyring-daemon` is absent.
- **`tools/keyring-check/qindaqt-keyring-check`:** read-only report of the
  provider owner, default-collection lock state (properties only — never
  Lock/Unlock/Prompt), the `pam_gnome_keyring.so` display-manager lines
  (including the `-auth` dash prefix), and the portal Secret routing row.
  Parsing is pinned by six fixture cases.

## Verification commands (all run, with results)

| Command | Result |
| --- | --- |
| `qq-reconfigure gap-keyring` | JOB OK |
| `qq-build gap-keyring tst_sessionenvironment qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_prompt_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_presence_tests qindaqt-network-secret-agent` | JOB OK (182 steps) |
| `qq-test gap-keyring 'qindaqt\.secret-service\|qindaqt\.keyring-check'` | exit 0 — 4/4 passed (provider 0.48s, wrong-password 0.68s, no-daemon 2.18s, keyring-check-parsing 0.38s) |
| `qq-test gap-keyring 'sessionenvironment\|network-secret-agent\|secret-service\|keyring-check'` | exit 0 — **12/12 passed**: session.sessionenvironment, all six qindaqt.network-secret-agent rows (controller, prompt, dbus, installed, boundary, boundary-poison), qindaqt.settings-network-secret-agent-presence, plus the four rows above |
| `./tools/validate-docs` | exit 0 — 239 documents and mkdocs.yml navigation validated |
| `mkdocs build --strict --site-dir …/gap-keyring/site` | exit 0 |
| `./tools/check-source-shape` | exit 0 (no new violations; pre-existing ones untouched) |
| `git diff --check 7dad9e78..7717c996` | exit 0 |

Bring-up honesty: the first three `qq-test` runs of my own rows failed while
the bootstrap was being worked out (3 failing → 2 → 1 → 0). The failure
signals were real daemon behaviors, now encoded as guards in the scenario:
`--start` is incompatible with `--unlock`/`--login` in gnome-keyring 48;
`busctl list` prints activatable names with a `-` pid; `Properties.Get`
replies are variants (`v b false`); and locking must go through
`Service.Lock(ao)` because gnome-keyring implements no `Collection.Lock`.
No JSON files were changed, so the `python3 -m json.tool` gate had no inputs.

## Deliberately left out

- **No session-environment code change.** No installed engine honours an
  environment variable for store selection, and redefining
  `XDG_CURRENT_DESKTOP` would change portal/GTK/Qt matching, which the lane
  forbids. `src/session/sessionenvironment.cpp` and its test are untouched.
- **No SSH agent session integration.** gcr-ssh-agent is installed; enabling
  it stays a user choice (ADR-0135 decision 5).
- **No edits** to `src/session_supervisor/**`, `src/services/portal/**`, the
  portal routing file, `packaging/**`, or the testing-harness page.
- **ZCode's own `--password-store=basic` pin** lives in its application
  bundle; only the vendor can remove it (a CLI flag is appended after it and
  should win, but this is a vendor-level caveat, documented in the ADR).

## Requests for the Program Manager

1. **Harness page rows** (I do not own `docs/wiki/development/testing-harness.md`):
   add rows for `qindaqt.secret-service-provider`,
   `qindaqt.secret-service-wrong-password`,
   `qindaqt.secret-service-no-daemon` (private-bus Secret Service provider
   contract; skip exit 77 without gnome-keyring-daemon; needs
   `gnome-keyring-daemon`, `secret-tool`, `busctl`, `dbus-run-session`) and
   `qindaqt.keyring-check-parsing` (fixture unit for the keyring-check tool).
2. **Launcher overrides on the installed desktop** (third-party `.desktop`
   files, outside lane ownership): add `--password-store=gnome-libsecret` to
   every Exec line that launches a sync-era Chromium/Electron application:
   - `/usr/share/applications/zcode.desktop`:
     `Exec=/opt/ZCode/zcode --password-store=gnome-libsecret %U`
   - `/usr/share/applications/sloom-studio.desktop`:
     `Exec=env SIGNAL_LOOM_ELECTRON_PANEL_MENU=1 sloom-studio --password-store=gnome-libsecret %U`
   - the main claude-desktop entry (verify its exact Exec line at integration;
     `/usr/share/applications/com.anthropic.Claude.desktop`).
   Do **not** add any flag to ChatGPT (`/usr/share/applications/chatgpt.desktop`);
   its Chromium 152 engine already selects the Secret Service. Firefox and
   Thunderbird need nothing.
3. **Installed-desktop checks**: run
   `tools/keyring-check/qindaqt-keyring-check` on the live installed QindaQt
   session; expected: `provider: gnome-keyring-daemon`, `default-collection:
   unlocked`, `pam-gnome-keyring: present`, and
   `portal-secret-route: routed` once the gap-portals Secret row lands
   (`src/services/portal/data/qindaqt-portals.conf` is that lane's file; my
   docs reference the row and will be accurate only after its integration).
4. Confirm the integrated tree still passes the 12-row ctest regex
   `sessionenvironment|network-secret-agent|secret-service|keyring-check`.
