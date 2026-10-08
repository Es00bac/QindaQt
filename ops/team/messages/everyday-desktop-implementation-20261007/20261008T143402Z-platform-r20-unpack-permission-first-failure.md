# R20 first private build failure and release

- Actual settings0 and exact-only pretend0; configured24/24/isolation/signing/gpkg retained.
- Build PID2401398/start48180003 exited1/23.777001s, held child reaped and PID absent; no timeout/output trigger. No CMake/compiler/artifact.
- Complete outer build.log and temp log copied into retained-metadata; first-failure-index.json binds actual raw.
- Actual unpack error: private PORTAGE_TMPDIR parent created0700/cabewse by worker preparation, so userpriv cannot create work. This is setup failure, not production/source failure. Installed source/runtime98342/archivee7fc/recipea37c unchanged.
- world/profile/normalR20 package controls unchanged; normal repository/checkouts before-after recorded. Comparison True.
- Own resources released; no retry, permission correction, helper/native/Portage action after failure.
- Proposal only: make just task-owned private PORTAGE_TMPDIR traversable to Portage with the same bounded ownership/mode as successful R19 directory, after exact source/harness review. Preserve all first failure/raw; separate retry logs and grant needed.
- Actual permission inventory first-failure-permissions.json; no feature/userpriv/sandbox/policy override proposed. No host permissions or global config change.
