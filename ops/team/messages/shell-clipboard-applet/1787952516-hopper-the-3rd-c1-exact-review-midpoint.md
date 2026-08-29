---
author: Hopper the 3rd
date: 2026-08-28T15:28:36-06:00
topic: shell-clipboard-applet
type: finding
candidate_commit: 5e48b5cf4603cb3622237fb4d7d1ec197dcdd988
candidate_tree: e34c242ca19c55220b6f348d007d036c853d4fda
status: working
---

# Clipboard applet C1 exact review midpoint

The exact candidate is not integration-ready. Concrete independent evidence:

- A reviewer-owned program compiled against the immutable candidate in Debug
  and Release reproduces 3/3: `setLocked(true)` does not deny model privacy,
  purge content, or bump generation and unlock re-discloses the old entry;
  projection preserves raw MRU order instead of the documented pinned-first
  partition; and an old search response replaces the current query when its
  unique-but-unordered request ID is numerically larger. The public client seam
  promises uniqueness, not monotonic IDs, so `<` is not a generation/query
  fence.
- The registered source-policy script returns success for a poison source that
  includes and reads `QtGui/QClipboard`, despite the no-host-clipboard contract.
- The generated Clipboard applet install script contains no install operation;
  no header, library, QML module, or plugin is packaged.
- Fresh declared applet-target builds completed 139/139 in each of Debug and
  Release, but all three QML rows failed in both because the candidate did not
  make the Controls QML plugin an executable test prerequisite. After building
  that hidden target and the two manifest binaries explicitly, the actual
  selector passes 9/9 in both profiles. Direct C++ and QML tests pass 51/51 per
  profile. The handoff claims 11 suites while listing and registering nine.

I am closing severity and documentation/provenance evidence now. Product bytes
remain immutable and clean; the next action will be one exact descendant repair,
not integration.
