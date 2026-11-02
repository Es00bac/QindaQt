# Keyring request transparency handoff

- Time: 2026-11-02T08:34:36-07:00
- Product commit: `539aecf5adf1b870d6ef97253e39a7c93177d4f0`, integrated on main and qinda hub.
- Overlay commit: `9030897f1214b3a130b5a71c4a364a6b041f6ccb`, integrated on master and qinda hub.
- Changed paths: `AGENTS.md`, keyring daemon/prompt/importer, keyring protocol and client, focused keyring/client tests, owning architecture/settings/testing wiki pages, ADR-0432, navigation and task/handoff records. The commit names every file.
- Product: request details and shared unlock scope precede password entry; Deny is initial focus; PID/UID/start-time and available executable are revalidated. Protected paths are explicitly unknown and require the labelled continuation; missing process identity refuses. Controls/bidi cannot rearrange displayed metadata. API-supplied task reasons are unavailable and are not invented. Agent workflow must announce actual tool/service/task/scope before authentication and keep unattended jobs noninteractive.

## Verification

- Strict focused Ninja builds: exit 0, using current `portageq envvar MAKEOPTS` (`-j24 -l24`) without configuration changes. Built daemon, importer, prompt, protocol/requester/QML/client tests and test-only native prompt helper.
- Five distinct CTest groups pass: `keyring_prompt_metadata` (5 Qt results), `keyring_requester_identity` (4), `keyring_prompt_qml` (10), `keyring_client` (9), and `keyring_request_review_journey` (one actual Wayland create/unlock flow, two visible review/approval steps, 5.21 seconds). All exit 0; the final four affected rows finish in 10.57 seconds. The client fixture now waits for object export after name acquisition.
- Five selected standard private-bus cases pass in 12.623 seconds: requester attribution, owner/cancel/disconnect, concurrent unlock sharing, restart/unlock and actual libsecret DH. Three native reveal/caller/rekey cases pass in 5.791 seconds. Integration-root attribution check passes in 1.572 seconds. No real wallet or credential read.
- `mkdocs build --strict` and `tools/validate-docs`: exit 0; 583 Markdown documents. Touched production files stay below 500 nonblank lines. `git diff --check`: exit 0.
- Rendered QML inspected at 1024×900 and compact controls checked at 640×480. The separate native smoke runs a private 800×600 virtual compositor with exact peer verification.
- Package Manifest generation and sequential application of all five recipe patches: exit 0. All 15 changed production files match the tested commit, except the CMake file retains the existing r45 activation-helper install stanza. Base archive verifies against r47 SHA512 and remains immutable.
- `pkgcheck`: exit 0 with inherited exact-6.6.6 LayerShellQt/KWayland availability and generic-profile solvability findings; dependency expressions are unchanged. Actual host `emerge --pretend --verbose --oneshot --noreplace` with the candidate overlay: exit 0, one r47→r48 upgrade, zero downloads.

## Adoption and limits

No signed Portage package build, system merge, real authentication or service
restart occurred. `sudo -n true` and `sudo -n -l` report that a password is required.
The remaining operator step is:

```sh
sudo -v && qinda-sync packages '=gui-wm/qindaqt-desktop-0.1.0_pre20261002-r48'
```

Sign out/in afterwards: mixed QMP1/QMP2 daemon/helper pairs intentionally reject.
The user is authorizing the package installation, not an unexplained keyring
request. Ordinary collections remain shared by same-user programs. Process paths
and ancestry do not attest software or agent intent, and the ordinary overlay is
not a secure-attention channel. No independent model review was launched, in
accordance with the user's explicit worker-budget instruction; this session
reviewed the exact source/recipe changes and their focused evidence.
