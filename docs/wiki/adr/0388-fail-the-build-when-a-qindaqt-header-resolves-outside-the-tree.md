# ADR-0388: Fail the build when a QindaQt header resolves outside the tree

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Build system; every module that installs public headers
- **Supersedes:** None
- **Superseded by:** None
- **Related:** [ADR-0379](0379-make-session-liveness-event-driven-and-self-healing.md), [Coding practices](../development/coding-practices.md#link-the-module-whose-header-you-include)

## Context

After the stability round integrated ADR-0379, the power service's lid and
critical-battery runtime tests failed on qinda while they passed at the base
commit. `qindaqt_power_service` linked `QindaQt::SessionActions` PRIVATE, but
`app/main.cpp`, compiled into `qindaqt-power-service` and the private lid
resident, includes `session_actions_client.h` and constructs the client. The
compiler did not find that header in the tree and fell back to
`/usr/include/qindaqt`, where the installed package kept an older copy. The
executable allocated and read the client with the old class layout while the
linked library used the new one: `m_running` read as 18, availability never
reached the policies, and the lid inhibitor and critical actions never armed.
The base commit passed only because its header happened to match the
installed one.

A `ninja -t deps` scan of the fully built integrated tree found the same
fallback in five more objects: `qindaqt-keyring` (`app/main.cpp` and its moc),
`tst_keyring_lock_policy`, `qindaqt_screen_power_service_tests` and
`qindaqt_settings1_screen_lock_settings_tests`. Their installed copies matched
the tree, so they worked by accident. A Portage build compiles against
whatever headers the build host has installed, so any of them could ship with
a mismatched class layout, and a build host without QindaQt installed cannot
compile them at all.

## Decision

1. **Link what you include.** A target that includes a `<qindaqt/...>`
   header links the target that provides it. A library whose headers expose
   another module's types links that module PUBLIC.
2. **The build enforces it.** `cmake/QindaQtHeaderIsolation.cmake`
   (option `QINDAQT_HEADER_ISOLATION`, default ON) writes an `#error` stub for
   every public header of the tree, and for every QindaQt header installed in
   the compiler's implicit system directories, into one directory passed to
   every compile with `-isystem`. Compilers search `-I` directories first,
   `-isystem` ones next and the standard system directories last, so a linked
   target never reaches a stub and an unlinked include fails at compile time
   instead of using an installed copy.
3. `qindaqt.header-isolation-guard` proves both halves on every test run:
   a module directory on `-I` wins over the guard, and the same include
   without it fails with the guard's message.

## Consequences

- The six objects above now compile against their modules' headers:
  `qindaqt_power_service` links `QindaQt::SessionActions` PUBLIC,
  `qindaqt_keyring_service` links `QindaQt::SettingsClient` and
  `QindaQt::SessionLockState` PUBLIC, and the two screensaver-fixture tests
  link `qindaqt_settings_screensaver`.
- A missing link is a compile error in every build, including Portage
  builds on hosts with an older QindaQt installed.
- Turning the option on or off changes every compile command, so the first
  build after either change rebuilds everything.
- AUTOMOC's dependency scan may still list installed headers in
  `_autogen/timestamp` files. Those entries only decide when moc reruns;
  moc output is compiled under the guard.
- MSVC builds skip the guard.

## Revisit when

The project stops installing public headers, or a toolchain is adopted whose
include search order does not put `-isystem` directories between `-I`
directories and the standard system directories.
