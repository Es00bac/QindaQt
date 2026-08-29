# Manager QST-1 combined qualification pass

- Time: 2026-08-27T14:30:50-06:00
- Exact code integration commit: `05a8636fb8ba9914e51d1cae5f117f77e90c75e3`
- Exact tree: `bf0e61dd1fad12bbb6498a943b69b17921e17656`
- Independent verdict: ACCEPT, P1/P2/P3 `0/0/0`
- Manager gate result: PASS
- Main fast-forward: pending preservation of unrelated overlapping checkout work

Manager evidence on the exact combined Settings + QST-1 tree:

- Debug and Release strict-warning production-shell builds: **906/906** each.
- Debug and Release QST-focused: **5/5** each.
- Debug and Release Settings-focused: **16/16** each.
- Debug and Release complete `^qindaqt[.]` registry: **87/87** each.
- Settings process daemon-loss lifecycle: 20 consecutive passes in each build.
- Fresh Release production/package build with testing disabled and an exact
  configure-time install prefix: **454/454**.
- Corrected clean stage: **168** files; Settings1 descriptor `Exec=` exactly
  equals the staged executable; no missing linked library.
- Installed QST QML consumer: **3/3**.
- Installed Settings two-daemon loss/reactivation/UnknownKey lifecycle: ten
  consecutive passes.
- Debug, Release, and production `all_qmllint`: exit 0 with the established
  unrelated shell warnings.
- Source shape: **789** files, zero violations; docs/link/nav: **44**; strict
  MkDocs, whitespace, exact HEAD cleanliness, and exact staged/build process
  cleanup all pass.

The earlier invalid late-prefix manager stage remains recorded in
`1787862131-manager-qst1-combined-qualification-checkpoint.md`; it was not
counted. The clean corrected stage above is the package evidence.

The shared `main` checkout acquired unrelated uncommitted team-board tooling
and wiki changes while gates ran. They overlap `docs/wiki/adr/index.md`,
`docs/wiki/index.md`, and `mkdocs.yml`, and propose a different ADR-0013 while
QST already owns accepted ADR-0013. The manager will not overwrite, stash,
stage, or absorb those changes. `main` stays at `c498269...` until that work is
preserved and rebased/renumbered. The qualified QST integration branch remains
clean and preserved meanwhile.
