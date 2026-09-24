# Audio1 owner refresh documentation repair re-review

Reviewed exact candidate 46a5124cbd6a6a756036f9c735b28e6061eb505f, descendant of 99b2fac10e4fe181473912625ae42677e36068ff, against manager main 8c639c1e38ca445f7d8a17430f6b2a510ffc6ced.

Verdict: ACCEPT. The prior ADR-0094 blocker is repaired.

- ADR-0094 now has a reciprocal “Extended by” link to ADR-0256. It preserves the original four-unit list as the ADR-0094 acceptance-time list, distinguishes the original Wayland/routing inclusion rule from the later Audio1 package-upgrade ABI exception, describes the current five-unit list and revised consequences, and requires future additions to update the owning ADR.
- New ADR-0256 records the observed old Audio1 ABI after package upgrade, the exact Audio1-only exception, the owner query → RestartUnit enqueue → old-owner retirement wait ordering, the two-second bound, and the continue-session failure posture.
- ADR-0256/index/nav entry is present in numeric order. ADR-0094 index/nav remains intact.
- ADR-0256 and the updated compositor/audio architecture text disclose the private-bus Type=dbus limitation: the manager may not retire the owner, the bounded wait reports it, session startup continues, and Settings may still see that owner. This is consistent with the code's best-effort behavior and does not claim guaranteed retirement.
- The candidate diff from the reviewed implementation contains only the ADR-0094, new ADR-0256, ADR index, and MkDocs navigation changes; no code/test behavior changed. The focused lifecycle test passed 1/1 (2.63 s) on the parent implementation commit; git diff --check passed.
- Merge preflight against 8c639c1e is clean: git merge-tree --write-tree produced 24fb0a5afabdd8ee8e88376a4148d445726b4740 with no conflicts.

No source or candidate files were changed during this re-review.
