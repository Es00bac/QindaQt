# Native miscellaneous portal delivery

- Status: working — implementing native Print, Account, DynamicLauncher and Usb from exact upstream contracts
- Base: `87b00d6e1560acd3eae3265693fbac44d4f0bf54`
- Branch: `worker/pf-portal-misc-20261002`
- Ownership: `src/services/portal/misc_families/`, focused tests, primary wiki/ADR0337, own board/thread; shared composition edits additive and coordinated
- Resources: source work on laptop; qinda compiler slot pending manager grant; no live printer/device/profile changes

## Updates

- 2026-10-02T17:01:42+00:00: Claimed four independent families; installed backend XML confirms frontend owns DynamicLauncher installation/token issuance, backend owns preparation consent and noninteractive authorization. Existing request/session/consent boundaries remain public and unchanged.

- 2026-10-02T17:12:15+00:00: Four request adapters and policies authored; helper/print conversion are source-complete pending compilation. Native FD, foreign parent, lock and frontend Close boundaries reused unchanged. Preparing focused injected policy/request/print tests; no physical devices or printer operations.
