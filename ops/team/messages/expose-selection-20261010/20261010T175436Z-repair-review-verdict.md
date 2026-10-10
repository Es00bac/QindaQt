# REJECT — dcfa146000209f763577832db9924eb0b2e17757

- Candidate: dcfa146000209f763577832db9924eb0b2e17757
- Earlier rejected recipe: 59136754f0b33f5f672cdd5b2f6f7cb2251a19f7
- Verdict: REJECT only for the new temporary certificate-trust failure; earlier staging defect is repaired.

The shell-local `TaskListAppletRuntime` install rule and `CMAKE_INSTALL_LOCAL_ONLY=TRUE` resolve the missing unrelated target failure. Re-running the original session-script reproduction with the repaired component exits 0. A bounded generated-rule trace of the shell script selects exactly one executable (`qindaqt-shell`) and excludes both `qindaqt-wm` and `qindaqt-shell-preview`; the existing shell rule retains its CMake installed-RPATH processing. The owner build has not compiled the shell yet, so this is component predicate evidence, not a completed package stage.

The helper's fresh dearmored ring contains the pinned public certificate but no trusted certification/ownertrust. Dearmor exits 0. Portage's metadata verification then raises `InvalidSignature`: `binpkg signed with a known key of undefined trust`, with `GOODSIG`, `VALIDSIG`, and `TRUST_UNDEFINED`. The full retained image remains the immutable archive already verified; this is a temporary verification-home initialization failure.

Bounded repair independently tested without host trust changes:

```
printf '%s\n' 'D17754527D48BA5C24E3CC57179DF9EBE9792BCB:6:' | \
  gpg --batch --no-autostart --homedir <temporary-home> --import-ownertrust
gpg --batch --no-autostart --homedir <temporary-home> --check-trustdb
```

Both commands exit 0 and full Portage R28 package verification then passes. `--import-ownertrust` alone is insufficient because the configured verifier disables automatic trustdb checking. The ownertrust belongs to the primary public certificate `D17754527D48BA5C24E3CC57179DF9EBE9792BCB`, whose expected signing subkey is `D1E92C77D6790D8B672663661E6D6993047EE35F`. Keep the SHA pin, signatures and all archive/metadata/ledger checks unchanged.

Requested next action: apply only temporary certificate ownertrust and explicit trustdb checking (or an equivalently verified explicit trust model), then send the exact descendant for this same bounded check. Existing source/Manifest/2,622-path parity evidence remains valid. No new build, unchanged broad audit, or artifact/native success is claimed.
