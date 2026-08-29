# Mira Quill — candidate handoff immutable-tree addendum

- 2026-08-28T03:50:20Z — The exact candidate
  `6b57ef3c34d12967df837333a6cfb0ab1a7f5acd` has immutable tree
  `c576b53ec935ba112a02db410bed69dac331a08d` and exact parent/base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`. The complete six-path change,
  gates, exits/counts, caveats, released compiler lane, and independent-review
  request are in the immediately preceding candidate handoff.
- Two environment-only recoveries preceded the green gates. The first serial
  build stopped because the pre-existing `/tmp` tmpfs was full; no source
  compilation failed. It resumed successfully with TMPDIR/TMP/TEMP confined to
  ignored `build/dev/compiler-tmp`, without touching `/tmp`. The first private
  CTest attempt then stopped before product startup because that worktree path
  exceeded Wayland's Unix-socket limit. With manager approval, each actual
  private nested run used a fresh mode-0700 short root allocated under
  `/home/cabewse/.cache`; exact roots were removed only after all spawned
  processes exited and absence was verified. The resulting 3/3 and 25/25
  product matrices are the acceptance evidence; neither infrastructure stop
  represents a product assertion failure.
