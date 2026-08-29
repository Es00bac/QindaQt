# Settings navigation exact review-repair handoff

- From: Ada the 3rd, Settings Center S1 recovery implementer
- At: 2026-08-28T15:55:58-06:00
- Status: handoff; not live
- Exact repair candidate: `95ebbd476acad4c3b40c67226e7298091527f0f5`
- Repair tree: `5c02f21a4f8d6d3267146048abf09c55a8ef6458`
- Exact parent/reviewed candidate: `7e6f133e280920f98fcb0ea79385d496b7871bd6`
- Original base: `0760e08e1118d6a8b8101f6d17d271d1b766cc96`
- Worktree/branch: `/mnt/d/QindaQt/worktrees/settings-center-navigation-s1-sylvie`, `worker/settings-center-navigation-s1-sylvie`
- Worktree state after commit: clean

## Terminal findings repaired

1. Wide and compact unavailable PageTabs now expose `modelData.unavailableReason` through their accessible descriptions.
2. Unavailable tabs remain focusable for assistive discovery and Escape return while their click activation is guarded. The fully token-published compiled page regression proves selected state, registered diagnostic, Escape focus in both 720×520 wide and 440×360 compact layouts, and no controller mutation when an unavailable compact tab is invoked.
3. Developer QML is authorized only when the runtime executable canonical path equals the exact compiled build executable. The installed-package row withholds its required staged Appearance module while the developer QML tree remains present, requires exit 3, restores and reinstalls the bounded component, then proves both complete staged routes. A build-root descendant can no longer borrow the build module.

## Acceptance evidence

- Strict Debug focused selector: 9/9 PASS, 0 failed, 13.86 seconds.
- Strict Release focused selector: 9/9 PASS, 0 failed, 13.36 seconds.
- Direct token-published compiled page with `QT_FATAL_WARNINGS=1`: 6/6 PASS, 0 warnings/failures.
- Mutation-sensitive installed-package row after the final script: PASS, 1/1.
- `git diff --check`: PASS.
- `./tools/check-source-shape`: PASS, 1,357 source files and zero allowlisted skips.
- `./tools/validate-docs`: PASS, 92 Markdown documents and MkDocs navigation.
- strict MkDocs output at `/mnt/d/QindaQt/builds/settings-center-navigation-s1-sylvie-docs-rereview`: PASS.

## Requested action

Noether the 4th should rereview exact immutable descendant `95ebbd476acad4c3b40c67226e7298091527f0f5` against terminal verdict `first-party-settings/1787953846-noether-the-4th-settings-s1-exact-review-fail.md`. Do not approve prose or the original commit; verify this exact tree and the mutation-sensitive regressions.
