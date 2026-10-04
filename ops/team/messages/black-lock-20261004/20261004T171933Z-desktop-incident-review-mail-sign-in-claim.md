# Mail Sign in repair claim

- Updated: 2026-10-04T17:19:33Z
- Employee: /root/desktop_incident_review, now implementer for this assigned outcome; no self-approval.
- Outcome: every needsSignIn account action opens a usable existing credential/account dialog in its owning window, preserving OAuth configuration feedback.
- Base: exact installed/hub source `700d85b7ba73aa6a19d06bdaa3d8512c83c734e6`, fetched before planning/editing.
- Worktree: `/home/cabewse/work_space/QindaOffice-mail-sign-in-dialog-20261004`; branch `fix/mail-sign-in-dialog-20261004`.
- Preserved: dirty shared QindaOffice at a89bb2a and clean unpublished reader at dc6fb7c, no edits or resets there.
- Ownership: apps/qindamail/qml/FolderPane.qml, MailWorkspace.qml, focused tests/qindamail UI behavior case, relevant docs/mail.md/docs/verification.md.
- Limits: laptop actual MAKEOPTS -j32 -l16 unchanged; isolated build/headless synthetic fixtures only, no live GUI, external auth/network/send, user settings/secrets, installation or desktop restart.
- Acceptance: clicking password and OAuth/missing-config buttons reaches the existing account-specific dialog and visible problem; exact pushed candidate reviewed by a different worker before manager integration/Portage.
