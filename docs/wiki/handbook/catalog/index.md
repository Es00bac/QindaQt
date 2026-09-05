# Exhaustive snapshot catalogs

> Repository snapshot: `9728612046940b55d69f85c3811eb38a08a0963b`. This catalog records checked-in contracts and evidence at that commit; it does not claim new runtime verification. Canonical linked pages remain authoritative as the project changes.

These catalogs make the complete checked-in feature ledger and reference inventories browsable by category. Start with a user-facing handbook topic, then use these indexes to locate the exact contract or source behind it.

- [Feature and evidence ledger](features.md): 7 features and 34 weighted steps, including every recorded stopping point, caveat, and evidence item.
- [Canonical wiki and ADR reading catalog](reading.md): 144 source pages, grouped by subject.
- [Settings keys](settings.md): 48 active-schema keys, exact JSON defaults, constraints, and packaged profile overrides.
- [Packaged assets](assets.md): every layout profile, theme, applet manifest, and default capability policy.
- [Repository navigation](repository.md): production modules, focused test families, tool files, and root build/operations entry points.

The inventories describe the source snapshot, not a release promise. A packaged setting, manifest, model, or candidate evidence record may exist before the corresponding live desktop behavior is fully qualified. Follow each feature's caveats and canonical page.


## Maintaining this snapshot

Run `python3 docs/wiki/handbook/catalog/verify.py` to check coverage against the recorded Git commit. The verifier checks every feature and weighted step, all 174 evidence references and summaries, every active settings key/default/constraint, canonical wiki link, packaged asset identity, and tool path. It does not run production tests. When intentionally refreshing the catalog, update the provenance and verifier base together and preserve state caveats; never relabel historical evidence as a fresh run. Integrated navigation still requires `tools/validate-docs` and `mkdocs build --strict`.
