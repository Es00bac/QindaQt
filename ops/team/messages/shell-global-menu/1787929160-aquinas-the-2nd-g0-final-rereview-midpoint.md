# Aquinas the 2nd — exact G0 repair rereview midpoint

- **Timestamp:** 2026-08-28T14:59:20Z
- **Candidate:** `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`
  (tree `43ca66cccea668cd0055072f2717457b394e43b6`, parent exact failed
  `79e7333de250cc7e3e4aa15df3c084789539f16f`).
- **Mode:** independent source/test/docs review only; worktree remains clean;
  no compiler, CTest, QML runner, GUI/session/input/config, Git mutation, or
  product edit.

## Confirmed repair closures so far

- `AuthenticatedProvider` is no longer public-mintable: its sole field
  constructor is private and friended only to `ProviderAuthenticator`, and the
  result carries it as an optional (`provider_authenticator.h:19-64`). The
  structural test now obtains all values from the real authenticator and checks
  non-default/non-aggregate construction (`tst_menu_ownership.cpp:57-73,
  130-140`).
- The focus-changing fake's counter is `mutable`, preserving the const seam and
  removing the deterministic compile defect (`tst_menu_ownership.cpp:294-324`).
- The exporter now retains last-good truth when same-epoch changed content
  reuses a revision, a revision regresses, or the epoch is null
  (`menu_exporter.cpp:56-92`; exporter tests `228-284`; composition test
  `192-222`).
- A destroyed `QMenuBar` now yields incomplete `source-destroyed`, and every
  adapter traversal failure has a distinct stable code with focused retention
  and class assertions (`qmenubar_menu_source.cpp:25-32, 87-165, 175-207`;
  adapter tests `226-351`).

## Material compatibility defect under final classification

The replacement parser is not the exact D-Bus bus-name grammar claimed by the
handoff/wiki:

- `provider_authenticator.cpp:43-46` allows only ASCII letters, digits, and
  underscore. D-Bus bus-name elements also permit `-`, so a valid daemon-issued
  unique name such as `:1.worker-2` is rejected before its bus credential can
  be authenticated.
- `menu_limits.h:22` sets the provider-name ceiling to 256 bytes and
  `provider_authenticator.cpp:26` rejects only values greater than that. The
  D-Bus maximum bus-name length is 255, so a 256-byte candidate is admitted.
- `tst_menu_ownership.cpp:242-292` covers well-known names, empty/single
  elements, and a space, but neither accepts a valid hyphenated unique name nor
  rejects the 256-byte boundary. The wiki repeats the narrower
  `[A-Za-z0-9_]` claim at `docs/wiki/shell/global-menu.md:82-86`.

Authoritative reference: the
[D-Bus Specification](https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names-bus)
states that bus-name elements allow `[A-Z][a-z][0-9]_-`, bus names must contain
at least one period, and the maximum name length is 255. Minimal repair is to
admit `-`, make the canonical ceiling exactly 255, add positive hyphen and
255/256 boundary cases, and correct the wiki grammar.

Continuing the requested presentation/accessibility, lineage edges,
CMake/install/CI, docs/ADR, and integration-collision review before the exact
P0/P1/P2/P3 verdict. This midpoint is not acceptance.
