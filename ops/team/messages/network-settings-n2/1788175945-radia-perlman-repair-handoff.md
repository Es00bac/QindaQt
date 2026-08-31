# Radia Perlman — Network Settings N2 repaired exact candidate handoff

- Timestamp: 2026-08-31T05:32:25-06:00
- Candidate: `6f5d0ba9915851195a4776b3a1e2f224c369a958`
- Tree: `38bfff6a125735166d4bec9d56fb134646d0ea12`
- Sole parent: rejected immutable candidate `43b563cdfe08455269375e2c356112902e562641`
- Original base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Branch: `worker/network-settings-n2`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2`
- Worktree state: clean; no ignored or untracked residue

## Repaired outcome

This descendant repairs all four P2 findings in Barbara Liskov's exact verdict:

1. `NetworkClient` now exposes one read-only operation-admission predicate
   whose value is identical to operation entry admission and remains closed
   across scheduled and in-flight snapshot refreshes, including success and
   invalidation refreshes. Settings scan/connect/disconnect availability
   consumes the same predicate, and focused fake-transport tests reproduce
   both previously enabled-but-rejected windows.
2. The primary page documentation is exact: connectivity and scan state are
   rendered, while exact owner/epoch/revision remain route-model lineage and
   focused-diagnostic values rather than claiming the technical owner is shown
   on the ordinary page.
3. Literal access-point BSSID is removed from the QML-facing route projection,
   with every projected AP row asserting that the hardware address key is
   absent.
4. The ADR-0055 source gate now rejects every slot and every invokable outside
   the exact four permitted intents, plus `TextField`, `TextInput`, `TextEdit`,
   and `TextArea`. Its negative test first proves the permitted surface passes,
   then proves bare/qualified text controls and evasive `enableWifiRadio` and
   `submitKey` invokables fail.

The same repair also closes Barbara's bounded P3 observations where local and
safe: removes the unused QML property and no-op test expression, adds paging
key coverage, proves real/stub metaobject surface parity, and makes the
NetworkModel link explicit while removing an unused QtQml link.

## Repair changed paths

- Public client admission boundary:
  `src/services/network_client/CMakeLists.txt`,
  `src/services/network_client/include/qindaqt/services/network_client/network_client.h`,
  `src/services/network_client/src/network_client.cpp`, and new
  `src/services/network_client/src/network_client_admission.cpp`.
- Network Settings projection/presentation:
  `src/apps/settings/network/{CMakeLists.txt,network_settings_model.cpp,qml/NetworkPage.qml}`.
- Focused Settings tests and source gate:
  `tests/apps/settings/network/{check_boundary.cmake,check_boundary_negative.cmake,network_settings_test_support.h,tst_network_page.cpp,tst_network_settings_model.cpp}`.
- Direct public-client regression:
  `tests/services/network_client/CMakeLists.txt` and new
  `tests/services/network_client/tst_network_client_admission.cpp`.
- Narrowly corrected normative docs:
  `docs/wiki/apps/network-settings.md` and
  `docs/wiki/architecture/network-service.md`.

No private Network service, libnm, host radio/network mutation, credentials,
secret-agent, persistence, shell, Display, session runtime, task/feature,
handoff, or queue path changed in the product candidate.

## Exact repair evidence

Focused mutations ran first:

```sh
ctest --test-dir /tmp/qindaqt-radia-network-debug --output-on-failure \
  --no-tests=error --parallel 1 \
  -R '^qindaqt\.(network-client-admission|network-settings-model|network-page|network-settings-boundary|network-settings-boundary-poison)$'
```

- Debug: exit 0, 5/5 passed.

The complete strict affected selector then ran serially in Debug and Release:

```sh
ctest --test-dir /tmp/qindaqt-radia-network-{debug,release} \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^qindaqt\.(network-(settings-model|settings-model-adversarial|page|settings-boundary|settings-boundary-poison)|settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
```

- Strict Debug: exit 0, 14/14 passed; zero build warnings.
- Strict Release: exit 0, 14/14 passed; zero build warnings.

The public-client/package/policy selector also ran in both configurations:

```sh
ctest --test-dir /tmp/qindaqt-radia-network-{debug,release} \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^qindaqt\.network-(client|client-admission|installed-header-consumer|boundary|boundary-poison)$'
```

- Debug: exit 0, 5/5 passed.
- Release: exit 0, 5/5 passed.
- This includes the installed external public-header consumer and both positive
  and poison source-boundary rows.

Static/freeze evidence on the committed bytes:

- `tools/validate-docs`: exit 0; 112 documents validated.
- `/tmp/qindaqt-display-repair2-docs-venv/bin/mkdocs build --strict`: exit 0.
- `tools/check-source-shape`: exit 0; 1,680 sources checked with only the same
  three unrelated pre-existing warnings and no new size warning.
- Expanded direct Network Settings boundary and Network N0 boundary scripts:
  exit 0.
- `git diff --check`, staged path/provenance audit, exact parent/tree audit,
  and ignored/untracked residue audit: exit 0.

No private nested runtime or physical/host networking was used.

## Bounded caveats and requested action

The deterministic evidence remains fake-public-transport, absent-private-bus,
offscreen, source-boundary poison, and relocated-package proof. It does not
claim physical Wi-Fi/Ethernet/radio behavior, host NetworkManager mutation,
credential entry, secret-agent qualification, persistence, shell applets, or
session-runtime integration. Credential-requiring activation remains outside
this route and fails closed when the resident public boundary cannot perform
it.

Please route exact commit `6f5d0ba9915851195a4776b3a1e2f224c369a958`
back to Barbara Liskov for exact recheck of the four repaired P2 findings and
the relevant focused/package gates. Recheck this immutable descendant, not the
handoff prose; return any blocking reproduction to the preserved worktree, or
route the accepted exact candidate to manager integration.
