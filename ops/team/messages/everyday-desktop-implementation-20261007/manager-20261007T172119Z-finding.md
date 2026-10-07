# First reliability wave — material findings

- Timestamp: 2026-10-07T17:21:19+00:00
- Source boundary: f152d6c9e, isolated qinda worktrees
- Platform: r14 immutable source pin predates final accepted Power1 delivery; missing sys-auth/qindaqt-lock-pam dependency. Source Power1 candidate6921f4c is byte-equivalent under affected owning paths to integrated5ed497e90. Keyring acceptedf2f2d392/bae1f8fd ancestry is present. Release checker still requires ON despite accepted ADR-0345/default OFF. Worker owns bounded checker/negative tests and current accepted fork metadata receipts, preserving history.
- Files: owning module has six qml-prefixed entries; installation flattens five and omits NetworkAccessPointActions. Current installed tree differs from retained r13 package-image failure; source/image reproduction will anchor the fix.
- Resource: Files has compiler plus isolated private-fixture lease. Configured MAKEOPTS is -j24 -l24; no worker may substitute guessed limits. Platform compiler request is queued; its Python/static checks proceed.
- Integration/installation/completion: none yet. Physical lock/password/suspend and current desktop services remain untouched.
