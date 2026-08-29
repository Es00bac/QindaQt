# Dorian Vale — virtual-desktop PID-binding repair rereview PASS

- Timestamp: 2026-08-28T12:18:15Z
- Reviewer: Dorian Vale, independent KWin/nested-session auditor
- Verdict: **PASS**
- Findings: **P0/P1/P2/P3 = 0/0/0/0**
- Exact candidate: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Exact tree: `3d703cde297a10b5c0dfc4b6ff1009240fa2ee45`
- Exact parent: reviewed FAIL `478435ef10024d3747d959f5bb198e60f9277c99`

## Immutable identity and scope

Commit, tree and sole parent match the handoff. The repair changes exactly five
declared paths, +86/−17, and its recomputed sorted path-manifest SHA-256 is
`8fa09a1d30b9672b8a7d0b7021a119cdf853564e2594c5e5c4d9b94877dcee1b`.
The full delta from public first parent `0a547df3` remains exactly 23 paths with
the unchanged manifest
`6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`.
The candidate worktree and repair whitespace check are clean.

## P1 closure — authenticated production-shell ownership

`validate_boot_evidence()` first validates the exact process topology and then
passes its current shell PID to final dock validation
(`tests/session/desktop_session_topology.py:422-425`). Every consumed
`scope=dock` record must carry a canonical positive decimal-string
`processId`; final evidence rejects it unless the parsed value equals that
authenticated shell PID (`:261-299`). Readiness still polls boundedly but now
requires valid PID shape without pretending it can authenticate identity before
the final complete evidence object (`:334-340`).

My independent replay proves:

- the shell-owned positive row passes;
- missing PID fails;
- `"0"`, `"-1"`, integer, bool, leading-zero, plus-prefixed and
  whitespace-prefixed representations fail canonical parsing;
- Dorian's exact forged `"999999"` reproduction fails;
- another authenticated live role's PID fails; and
- the old dock PID fails after authenticated shell replacement.

The committed positive/hostile regression matrix is at
`tests/session/test_desktop_session_topology_unit.py:198-243`. I found no
repair regression or alternate foreign/stale acceptance path.

## P3 closure — normative reference

The method summary now names the exact notification/dock allowlist, the section
heading is scope-neutral, the body states the same missing/malformed/foreign/
stale dock-PID rule, and ADR-0026 is linked beside ADR-0020 without changing or
weakening ADR-0020's two-role Notification boundary
(`docs/wiki/reference/compositor-control-v1.md:44,194-224`). ADR-0026 and the
testing authority carry the matching authenticated production-shell contract.

## Fresh source-safe evidence

- Desktop-session unit discovery: **43/43 pass**.
- Independent positive and 11 hostile PID representations: pass/reject exactly
  as required.
- Nine desktop-session Python sources compiled in memory: pass.
- Exact Compositor1 XML/service descriptor check: pass.
- Documentation/navigation: **64 documents**, pass.
- Source shape: **993 files**, zero skipped/issues.
- Exact identity, ancestry, both manifests, diff check and clean tree: pass.

No CMake configure/build, package row, bubblewrap, private boot, private bus,
compositor, session, display/input, UI, host endpoint, host configuration or
hardware action ran.

## Decision

The prior blocking P1 and nonblocking P3 are closed. This exact candidate is
accepted for the manager-controlled isolated private-boot gate. It does not yet
have live desktop evidence or maturity; the runtime lane remains prohibited
until explicitly allocated by the manager.
