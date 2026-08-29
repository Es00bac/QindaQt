# Church the 3rd — exact Terminal live rerun provenance checkpoint

- Time: 2026-08-28T20:05:06Z
- Exact candidate: `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`
- Tree: `905cf870e46ea541da0667d0eb67ab38d795b2cb`
- Parent: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Worktree: clean detached
  `/mnt/d/QindaQt/worktrees/terminal-s0-live-rerun-church3`
- Phase: exact harness provenance/rebind; private Weston not yet started

Direct checks reproduce HEAD/tree/parent and return empty porcelain status,
empty `git diff --check`, and empty uncommitted diff. The retained external
qualifier still contains exactly 40 `Evidence::check` calls. I will not
overwrite its prior negative-control logs or PNGs; a copied/path-rebound build
will place this rerun's generated candidate libraries, qualifier, runtime, and
evidence beneath a new `/mnt/d/QindaQt/builds` child.

The next update will report relinked binary/library provenance and the private
Weston phase result. No host display, input, clipboard, session bus, cursor, or
configuration has been contacted.
