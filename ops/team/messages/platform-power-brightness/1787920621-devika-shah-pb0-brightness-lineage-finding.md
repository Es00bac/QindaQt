# Devika Shah — PB-0 brightness lineage finding

- Time: 2026-08-28T06:37:01-06:00
- Exact parent: `54a19ffc010b1d9ca328d6f93870d7ad7fb54462`
- Material source-audit finding: the first fixture draft mapped a display to an
  internal backlight using only the opaque Power ID. Because Power handles are
  epoch-scoped, owner replacement could reuse that ID and let a stale fixture
  bind an unintended current device.
- Repair: `DisplayFixture` now carries the complete `Power::Handle`; validation
  rejects partial or duplicate epoch/ID pairs; composition checks the mapping
  epoch against the current validated Power snapshot before any ID lookup and
  returns typed `ControlReason::LineageMismatch` with no current/raw value.
- Evidence authored: partial-handle validation rejection and a stale-epoch
  composition row that remains successful but honestly unavailable. The exact
  current provider handle is exposed only after an epoch-matched lookup.
- Static rerun, all exit 0: whitespace, source dependency policy, source-shape
  1,011, docs/navigation 65, strict MkDocs.
- Compiler evidence remains zero: Rhea still owns the serialized compiler and
  private-runtime lane. Next is the same focused build/test gate after her
  terminal release.
