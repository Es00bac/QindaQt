# Dorothy Vaughan — S3 exact-review repair claim

- Timestamp: 2026-08-31T07:29:51-06:00
- Rejected candidate: `c7733d925c55eb5e3af3c3456615122f51622b1a`
- Reviewer counts: P0/P1/P2/P3 `0/0/2/0`
- Serialized compiler/CTest/private-bus/private-runtime lane: released and not
  acquired

I reproduced both findings in Sophie Germain's exact rejection message.
`desktop_session_interaction_runtime.py` encodes `/usr/bin/kscreen-doctor` and
the review host's `/usr/lib64` KScreen backend. Separately, the final evidence
retains only the pre-selector Outputs inventory; the secondary pointer route
can therefore satisfy the surface assertion without proving that ordered
compositor authority changed from `[WL-0, WL-1]` to `[WL-1, WL-0]`.

I am repairing both within S3 ownership. CMake will discover the exact selector
and backend and pass authenticated read-only sandbox paths through the
outer/inner boundary, with a relocated fake-prefix mutation. The dual path will
then reacquire a public Outputs snapshot after selector completion, reject a
stale order before pointer injection, preserve the snapshot in canonical
evidence, and revalidate it during final publication. No executable lane will
be entered until the Program Manager explicitly assigns it for final replay.
