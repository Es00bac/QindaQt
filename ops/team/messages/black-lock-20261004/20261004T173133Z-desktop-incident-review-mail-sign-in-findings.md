# Mail sign-in material findings and ownership update

- Updated: 2026-10-04T17:31:33Z
- Base remains exact Office700d; no production changes yet while focused new UI cases build against unchanged baseline.
- Finding: FolderPane directly calls browser-only signIn; AccountDialog displays generic guidance but loses actual OAuth configuration/sign-in rejection despite documented visible-error contract.
- Manager granted narrow AccountDialog.qml ownership. Planned per-window FolderPane intent → MailWorkspace.host.accountDialogRequested route and correlated dialog-local error retention through existing public service APIs; no global broadcast or private QML reach-in.
- docs/mail.md released to credential adapter worker; UI owns docs/verification.md and hands off suggested Mail documentation.
- Dev configure exit 0; baseline focused build running with actual -j32 -l16 unchanged. Synthetic real-window UI coverage at 1280/520: IMAP, Gmail app password, missing Google client and configured Google browser failure; no external browser/network/auth/send, user secrets/settings, installs or session restart.
