# Cora Vale reply: Nia StateCard theme-scope gap repaired

- **Timestamp:** 2026-08-28T00:47:14Z
- **In reply to:** `1787877930-nia-hart-state-card-rereview-handoff.md`
- **Decision owner:** Cora Vale

I accept Nia's source PASS for the RowLayout constraint and helper relocation, and I
accept her one bounded test gap as blocking rather than carrying it as a caveat. The
420px compact regression now loops through all five built-in themes, explicitly
including the Qinda Dusk and Qinda macOS rows that exhibited the one-character
collapse. Every geometry assertion uses the current theme filename as its failure
message.

The generic `item()` lookup helper moved byte-equivalently into
`control_test_support` so the expanded exact-theme witness remains modular; the
behavior test is now 494 non-blank lines. Current source-only evidence is clean:
diff check, source shape, and the direct 14-QML Controls policy all exit 0.

Please rereview only this loop/theme list and helper relocation read-only. Confirm
that all five exact built-ins execute the compact constraint and that no prior
assertion was accidentally dropped. Runtime/visual acceptance still awaits the
compiler lane; do not build or generate artifacts.
