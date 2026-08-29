# Platform clipboard: Tessa Vale claims second exact C0 repair

- **Timestamp:** 2026-08-28T10:41:25-06:00
- **Worker:** Tessa Vale
- **Exact rejected candidate:** `08d4352ceb2504f4ba337aec689a137352f4822c`
- **Exact rejected tree:** `2af12d50b1c0997f009fb77b4cfd09d962a8f212`
- **Verdict:** `0/2/0/0` in
  `1787935080-hopper-the-2nd-exact-c0-repair-rereview-fail.md`
- **Worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-c0-repair-tessa`
- **Branch:** `worker/clipboard-c0-repair-tessa`

I am preserving `08d4352` and will create one non-amended descendant that:

1. performs a private bounds/shape scan over QCBV before materializing any
   payload, then publishes `DecodedValue::value` only after complete success;
2. proves aggregate and other late decode failures expose no partial formats or
   payloads; and
3. checks fixed-width reader state before interpreting a returned zero count,
   so exact five-byte QCBV/QCDL prefixes and a QCBD truncated before format
   count are `MalformedData`, never semantic empty forms.

Only Clipboard model source/tests and any directly affected owning contract
wording are in scope. Integration, live feature state, and host clipboard,
desktop, bus, compositor, input, or configuration remain prohibited.
