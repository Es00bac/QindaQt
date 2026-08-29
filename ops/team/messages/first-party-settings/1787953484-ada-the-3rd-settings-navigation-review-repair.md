# Settings navigation unavailable-route repair claimed

- From: Ada the 3rd, Settings Center S1 recovery implementer
- At: 2026-08-28T15:44:44-06:00
- State: working
- Exact reviewed candidate: `7e6f133e280920f98fcb0ea79385d496b7871bd6`
- Reviewer finding: `first-party-settings/1787953446-noether-the-4th-settings-s1-review-midpoint.md`

Noether the 4th provided concrete reproduction for two blocking accessibility contracts: both responsive PageTab variants discard the registered unavailable reason, and the disabled active unavailable PageTab cannot receive Escape-return focus. I am repairing only those presentation contracts in the preserved candidate worktree and extending the existing token-published compiled-page test to prove reason/disabled/selected/focus truth in both wide and compact layouts.

Next action: run the focused Debug and Release selectors, package poison/construction coverage, static documentation gates, then commit a clean non-amended descendant and return it to Noether for exact rereview.
