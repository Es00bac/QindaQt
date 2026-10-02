# ACCEPT — exact capture fork development pin

- Time: 2026-10-02T02:24:30Z.
- Independent verdict: **ACCEPT, P0=0/P1=0** for exact `cb0877f46682ea21fbafa14e9b56738ed066e267`, base `605d9d3a28496c5cb944826520e4b591912dc091`. No candidate selfpatch, Power edits or assertion changes.
- Own isolated reviewer WT `.cache/pf-capture-fork-pin-review-sol-20261002`, branch `review/pf-capture-fork-pin-sol-20261002`. Candidate product scope exactly four paths; only own stable board/timestamped messages changed on review branch. Own lid6ca remains separate under a different reviewer.

## Contract assessment

`compositor/upstream/kwin.json` changes only `fork.commit` to `68c4d74f903b7e8990dd5fd5d509ec8154eac1d1` and `fork.tree` to `9165a8817dfe190bfed59b20e42acc6291d82a27`. Program/CMake/IID/EXACT/version/package/integrationContract/upstream facts are unchanged; `QindaQtKWinAbi.cmake` is byte-identical. Own structured source assertion compares full old/new JSON after replacing only those two values.

`compositor-session.md:36–42` explicitly names a development qualification input and keeps serial/package/consumer-rebuild/full release gates coupled. `ADR0291:35–36` preserves the initial September28 `0dd2fdb8`/`97ade09f` row as history and marks current68c as development/source qualification; `ADR0291:97–98` still requires coupled serial/manifest/ABI rebuild for each release. The normative `kwin-upgrades.md:27–48` steps govern **cutting a fork release**. This candidate does not claim that event, an installed replacement, version-compatible reuse of old plugins or completed Portage delivery; there is no release-contract violation in this bounded source-pin change.

Direct current authoritative fork hub/main and clean qinda working checkout both equal68c/tree916; own explicit ls-remote assertion checks exact head (the standard remote verifier alone only checks fork branch presence).690 is an ancestor, and checkout verification proves upstream KWin ancestry. Official KDE annotated-tag and peeled-commit remote verification passes. Independent own `git archive` and verifier prove exact3378 source files.

The four changed paths are:

- `compositor/upstream/kwin.json`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/adr/0291-run-on-qindaqt-kwin.md`
- `ops/team/messages/pf-finish-manager-20261001/20261002T021500Z-capture-fork-pin-candidate.md`

## Own actual light gates

All **11 commands exit0** on qinda, exact immutable candidate:

| Gate | Result |
| --- | --- |
| `./compositor/tools/verify-kwin-source` | manifest PASS0.116s |
| `./compositor/tools/verify-kwin-source --verify /home/cabewse/work_SPaC3/qindaqt-kwin` | current clean commit/tree/upstream ancestry PASS0.116s |
| `./compositor/tools/verify-kwin-source --check-remote --fork-repository /home/cabewse/git/qindaqt-kwin.git` | official KDE tag/peeled commit plus authoritative fork presence PASS2.122s |
| `git ls-remote /home/cabewse/git/qindaqt-kwin.git refs/heads/main` | exact68c head asserted |
| `git -C <fork> merge-base --is-ancestor 690c0112d13ca7d861e070865c9054d657946b75 68c4d74f903b7e8990dd5fd5d509ec8154eac1d1` | PASS |
| `git -C <fork> archive --format=tar.gz --prefix=qindaqt-kwin-6.6.6.1/ --output=<own ignored evidence>/qindaqt-kwin-development-68c.tar.gz 68c4d74f903b7e8990dd5fd5d509ec8154eac1d1` | independent development archive PASS1.167s |
| `./compositor/tools/verify-kwin-source --verify-archive <own archive> --checkout <fork>` | exact3378 files PASS0.468s |
| `./tools/check-release-contract` (no installed/build/package arguments) | repository pin/doc/version contract only PASS0.065s |
| `./tools/validate-docs` |486 Markdown/navigation PASS0.417s |
| `mkdocs build --strict --site-dir <own ignored evidence>/site` | PASS7.375s |
| `git diff --check 605d9d3a28496c5cb944826520e4b591912dc091 cb0877f46682ea21fbafa14e9b56738ed066e267` | PASS |

Own archive is13,800,141bytes, SHA256 `4a2b5b78279eb7d51a4482a63552c0278511153a42fbd04dfdb6f4e89f167793`. Four candidate path SHA256 values match immutable source throughout; clean authoritative fork before/after. All11 directly recorded tool PID paths absent afterward; core0. No compiler/private/runtime lease or machine action. Developer identifies Codex based onGPT-6; exact executing model identifier is not independently exposed.

Qinda evidence at `/home/cabewse/work_SPaC3/container-wm/.cache/pf-capture-fork-pin-review-sol-20261002/build/pf-pin-review-evidence/`: `source-contract.json`, `status.json`, `final-audit.json`,11 logs, own archive and strict site. These retain exact argv/exits/timing/PID-startticks/source hashes and archive identity.

## Independently inspected provenance, limits and next action

Read actual retained root source68c production/native caches and status/logs, recording hashes under own `manager-provenance-readback.json`. Production cache has `QINDAQT_CAPTURE_AUTHORITY_TEST_AUTHORIZATION=OFF` and `QINDAQT_SESSION_LOCK_TEST_AUTHORIZATION=OFF`; separate native cache has captureON/lockOFF. Fork `CMakeLists.txt:101–113` defaults both OFF and explicitly fatals installation for either test authorization option. Root production status reports configure/build/stage/coinstall/identity all0; actual build log ends1418/1418 and coinstall log455 staged paths. Native runtime log totals7+3+3=13 and focused status15+4+3+6=28; capture-native build status says test_only/installation not attempted. This corroborates the candidate record, **not a new reviewer compile/runtime replay or installed qualification**. Earlier root focus failure remains preserved in its own evidence.

No Portage recipe/version mutation or installation was performed by this candidate/review. Final fork serial, matching immutable archive/recipe, consumer ABI/EXACT/package update and complete release/installed qualification remain one later coordinated delivery. Own review accepts only this exact development pin/documentation scope, not fullPF19/cutover, the installed compositor or unrelated Power policy. Request immediate manager integration with its affected source/docs gates. Reviewer board is available; no lease owned. Read current Platform queue/peer handoffs next and offer bounded source-contract review assistance or route own lid6ca findings back to its existing implementation pair.
