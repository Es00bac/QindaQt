# W0 network secret-agent repair worker

- Status: working — final build and requested suites pass; preparing the candidate and exact handoff evidence.

## Updates

- 2026-09-24T06:45:10Z — Claimed the repair at candidate `548be376e3cdd98c25edcc8894336481a0c9dcdb`; auditing admission, policy, recursive wiping, and private-bus coverage before edits.
- 2026-09-24T06:57:25Z — Repaired direct wire byte/string cleanup and policy round-trip temporaries; found qinda's Qt 6.11.1 suppresses isolated surrogates in `QString::toUtf8()`, so the explicit three-byte U+FFFD admission rule is tested separately from valid-string Qt comparisons.
- 2026-09-24T07:03:17Z — Target build passes; the secret-agent D-Bus, IP-config, and lifetime tests pass five repetitions (15/15 executions), the broader network/Settings Network selection passes 39/39, `tools/validate-docs` validates 388 Markdown pages and navigation, and `git diff --check` is clean.
- 2026-09-24T07:06:05Z — Committed product candidate `b6fb166b10ffb5476f841eeaff1feb7def7f4251`; exact commands, audit, bounded caveats, and request for independent review are in the handoff message.
- 2026-09-24T07:50:32Z — Reopened the existing worktree at `5ac3325e9e92cb50a1af283205940fbea5af7e78` for B3's manager ruling; the claim is recorded in `20260924T075032Z-luna-w0-repair-claim.md` and the admission guards/request-data regression are in progress.
- 2026-09-24T07:55:14Z — The request-retention regression found and isolated a live alias: the retained `PromptRequest.settingName` shared the settings-map key that the input wipe scrubs. Added an independent prompt setting-name copy and recorded the finding in `20260924T075514Z-luna-w0-repair-finding.md`; rebuilding and rerunning now.
- 2026-09-24T07:58:03Z — The post-reply assertion also found `SecretReply.settingName` and `SecretValue.key` sharing with live prompt metadata during reply-map cleanup. Both reply strings now take independent storage; the second finding is in `20260924T075803Z-luna-w0-repair-finding.md`.
- 2026-09-24T08:03:06Z — The final affected-target build passes with `-j8 -l20`; the focused D-Bus/IP-config/lifetime selection passes 15/15 executions and the broader network/Settings Network selection passes 39/39. `./tools/validate-docs` validates 388 Markdown documents and navigation, and `git diff --check` is clean. The initial regressions were corrected; strict MkDocs is unavailable on qinda.
