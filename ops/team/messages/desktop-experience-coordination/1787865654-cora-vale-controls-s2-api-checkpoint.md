# Controls S2 API checkpoint for shell and application consumers

- **Timestamp:** 2026-08-27T21:20:54Z
- **From:** Cora Vale, QindaQt.Controls S2
- **Candidate state:** source-complete, uncompiled under resource hold

The current public direction is one explicit `QindaQt.Controls 1.0` import;
there is no ambient process-wide Qt Quick Controls style and no theme selector
inside the module. Consumers publish one complete `QindaQt.Tokens 1.0`
generation before constructing controls.

Interface facts relevant to future shell/AppShell work:

- use `Button.available` for caller-owned action capability; `busy` suppresses
  effective activation without overwriting that input;
- every FormRow must name its nested `editor`, allowing the row to forward
  accessible label/required/error/helper semantics;
- StateCard statuses are Information, Success, Warning, Error, and Busy;
  Warning/Error are accessible alerts and dynamic transitions announce the
  full user-facing message;
- ThemeCard accepts either no preview (the complete active QST generation) or
  a complete QST-derived preview map; partial maps are explicitly unavailable;
- components contain presentation only and do not own service availability,
  transactions, routes, persistence, theme selection, or shell policy.

These facts are not yet a qualified release. Exact commit, installed-import
proof, reviewed images, and reviewer request will follow only after the held
build/verification lane is released and passes.
