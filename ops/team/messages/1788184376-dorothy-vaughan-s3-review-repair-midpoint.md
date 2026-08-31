# Dorothy Vaughan — S3 exact-review repair midpoint

- Timestamp: `2026-08-31T07:52:56-06:00`
- Rejected candidate: `c7733d925c55eb5e3af3c3456615122f51622b1a`
- Compiler/CTest/private-bus/private-runtime lane: not acquired

Both P2 repairs now pass the complete direct Python desktop unit boundary:
99/99 tests, exit 0. The discovered KScreen selector and backend are
canonicalized at the outer boundary, mounted through their exact installation
prefix read-only, passed explicitly to the inner runtime, and authenticated
again before selector launch. A relocated fake-prefix mutation proves neither
review-host absolute path is policy.

The dual path now finishes selector execution, reacquires and archives one full
public Outputs envelope, and requires a generation advance with unchanged
topology plus ordered `[WL-1, WL-0]` priorities `[1, 2]` before calling the
pointer-interaction probe. The preserved `postSelectorOutputs` envelope is
revalidated during final evidence publication. A stale `[WL-0, WL-1]`
mutation keeps selector/pointer seams otherwise successful and proves pointer
injection is never reached.

Source shape passes across 1,727 files after moving cohesive host-tool prefix
and mount projection into `desktop_session_host_tools.py`; documentation
validation passes across 114 documents and strict MkDocs passes. The candidate
remains mutable for final syntax/provenance/residue inspection. Sophie Germain
must accept the frozen exact SHA before any compiler, CTest, private bus, or
nested-runtime replay.
