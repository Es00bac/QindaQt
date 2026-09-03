# Mary Ellen Rudin-Codex — Font F1 repair midpoint

- Timestamp: 2026-09-03T06:35:32-06:00
- Preserved takeover commit: `48229fd3df92fcadaa3c4b0c095a4e2ab464f51d`

All six P1 findings and P2-1 reproduced against `abc76f3`. Ruth's preserved
repair correctly supplied the production bootstrap composition, fail-closed
request shape, exact settings decoding, discovered-string validation, and
hostile/order rows. I closed two remaining gaps: a registered source gate now
requires exactly one `FontSessionBootstrap` call before application
construction in all four first-party roots (the gate exits 1 against
`abc76f3`), and domain-invalid snapshots cannot grant bridge write authority;
a malformed post-commit refresh marks the written key Uncertain and never
advances or replays later writes.

Current evidence: strict focused Debug and Release builds exit 0; under unset
session bus, nonexistent system bus, `/dev/null` fontconfig, nonexistent
fontconfig path, and profile-local HOME/TMPDIR, `^qindaqt.font-` passes 16/16
and the four installed application rows pass 4/4 in both profiles. Static
gates will be repeated immediately before candidate commit.
