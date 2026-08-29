# Euler the 3rd — Network N0 manager-replay exact review midpoint

- **Timestamp:** 2026-08-28T21:04:43Z
- **Exact candidate:** `c9731a4e29b76cdde6aee25b5ba9bc5f39baa2d8`
- **State:** immutable different-worker review continues; no terminal verdict yet

The replay structure is independently sound so far: exactly two commits from
manager base `542dcac`, 56 changed paths, zero deletions and zero `ops/team/**`
paths. All 49 non-shared Network leaf blobs equal accepted repair `c619acd`.
Each of the seven shared paths has additions and zero deletions relative to the
manager base, preserving its Terminal, Text Editor/AppShell, Font, Clipboard,
and other current registrations.

Fresh strict-warning Debug and Release focused builds each pass **64/64**.
The exact registered selector passes **13/13** in both configurations, including
the isolated installed consumer, clean boundary, and policy poison. Direct
QtTest execution passes **118/118** per configuration; the adversarial row is
**10/10**, covering decoded-owner mismatch, A→B→A retirement, unbounded
lease, diagnostic cap, quoted secret, Unicode format control, false `wireValid`,
and failed-start retry.

Mutation sensitivity was attacked in a separate same-hash throwaway worktree.
Eight individual source mutations, one per repaired branch above, each caused
its exact hostile assertion to fail. After reversal and rebuild the complete
adversarial row returned to 10/10, and both the throwaway mutation tree and
immutable candidate tree are byte-clean. Remaining terminal checks are staged
package inventory and outside-prefix rejection, docs/MkDocs/source shape,
policy/provenance/fsck/path scans, and final exact cleanliness.
