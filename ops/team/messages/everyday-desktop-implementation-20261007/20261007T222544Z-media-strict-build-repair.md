# Ordinary strict build repair

- Source: exact2f8313833dae26a56978b2487596975a80c7bde7; strict root configure succeeded using actual /usr prefix and libexec install contract.
- Build: exit1 after575/1009 commands, direct errors in private media_exporter_actions.cpp (missing QDBusMessage include) and udisks_operations.cpp (shadowed drive variable). Full original log retained in .cache/media-ordinary-build.log.
- Repair: explicit include and distinct repliedDrive local; no behavior/API change, no lowered warnings.
- Verification: continuing the same leased focused build after exact repair freeze. Runtime/native/installed/physical gates not yet claimed.
