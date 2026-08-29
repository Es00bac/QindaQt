# Linus the 2nd — Text Editor AppShell repaired-descendant midpoint

- Timestamp: 2026-08-28T19:53:27Z
- Exact candidate/tree: `75f786e91a1877b9eb9fa0e2750fc2ddac1a9d80` / `65ea635651674dac73106019d61a845096d24280`
- Product worktree: detached, read-only and clean

All three former P1 findings are represented in the exact repair and their
first independent executable/static checks are green:

- Fresh strict Debug explicit editor build passed. The registered
  `^qindaqt\.editor` selector is now **10/10 PASS**, including the new policy
  and staged component rows.
- Direct Debug `qindaqt_editor_app_shell_tests -txt` is **11/11 PASS**. The
  named hostile cases exercise the real `EditorWindow`/bridge seam: typed Open
  and Save As cancellation, unchanged untitled/clean state, clear ambient
  error, subsequent request reuse, wrong-ID `StaleRequest`, one exact result,
  and opening only the exact path.
- Direct policy execution reports 11 actual AppShell bridge/window files and
  confirms its child-process poison rejection. The test is registered under
  the editor selector and scans the full nine-file app-shell subtree plus
  `editor_window.{h,cpp}`.
- Static documentation review confirms `EditorWindow`'s production
  `NativeFileSelectionAdapter` is distinguished from the bridge's null-only
  fail-closed fallback. The staged payload now truthfully names the AppShell,
  Controls and Tokens shared libraries while excluding unrelated binaries and
  plugins.

No blocker exists at midpoint. Release 10/10 and direct hostile proof,
Debug/Release adjacent 17/17, independent staged manifest/RPATH inspection,
source shape, strict docs/MkDocs, ancestry/diff/provenance and final clean tree
remain in progress. No host desktop, input or session resource is being used.
