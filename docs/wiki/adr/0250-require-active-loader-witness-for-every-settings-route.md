# ADR-0250: Require an active Loader witness for every Settings route

- **Status:** Proposed
- **Date:** 2026-09-23
- **Owners:** Settings Center
- **Supersedes:** None
- **Superseded by:** None

## Context

The Settings Center registry has 21 routes, but its construction script named
only 14 and accepted a process that stayed alive for three seconds as proof.
A page Loader could fail while the navigation window remained resident. The
script also poisoned the session bus but left the system bus available.

## Decision

The executable's `SettingsRouteRegistry::createDefault()` remains the only
ordered route inventory. An internal read-only `--list-routes` command emits
its canonical IDs. Both build-tree and staged-package construction checks use
that output and assert the current count, uniqueness, and ID shape. Adding a
route extends the registry and automatically subjects it to the same package
construction check; the count assertion makes accidental route removal
visible. Existing registry order and digit shortcuts do not change.

A separate internal `--route-construction-probe` command starts the requested
route, observes the one presentation-active `SettingsRouteHost` Loader, and
exits only after it has a real item in Ready state. A registry-declared
unavailable route may instead report an instantiated diagnostic page. The
witness carries the exact requested route ID and `ready` or
`diagnosed-unavailable`; a mismatched or missing witness cannot pass. A
Loader failure, unexpected QML warning, early exit, or timeout fails the
harness. A fake executable that remains resident without a witness is a
negative control; a fake ready witness with a QML warning is rejected too.
The construction subprocess has isolated XDG roots and
unreachable session **and** system bus addresses; the staged check runs from
only the installed Settings component after module-poison checks.

The QML host owns Loader selection, a small witness item observes readiness,
and a private Settings Center helper owns the package-test output and exit.
The probe destroys its QML roots before its route-model stack unwinds so
shutdown cannot produce false QML binding warnings. Normal launches do not
attach the probe, print witness lines, or exit on route construction. This is a
package-test interface, not a user-facing route setting or a new service.

## Consequences and limits

The gate proves each registered route's page or intentional unavailable
notice can be constructed in a confined offscreen process. It does not prove
that controls inside the page work, that a backend is healthy, that live
settings writes succeed, or that keyboard/assistive interaction and physical
compositor behavior are correct. Those remain separate focused, private-bus,
installed-session, and hardware gates. The extra registry command is a
read-only diagnostic; it does not expose route internals or change selection.

## Source-shape review

`SettingsRouteHost.qml` had 391 non-blank lines before this change. Six later,
independent route Loaders now live in `SettingsRouteSupplementalLoaders.qml`,
leaving the host at 337 non-blank lines: below the 350-line error limit but
still above the 275-line decomposition-review threshold. The remaining host
owns the single-Loader selection chain, shared active/inactive policy, and
Customize departure/close handling. Splitting those coupled invariants across
another component for this test slice would increase cross-component state;
the warning is recorded for a future route-host refactor. `main.cpp` is 433
non-blank lines after its private probe moved to a focused C++ collaborator.
