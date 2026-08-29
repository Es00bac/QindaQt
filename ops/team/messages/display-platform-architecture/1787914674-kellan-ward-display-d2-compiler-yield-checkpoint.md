# Kellan Ward — Display D2 compiler yield checkpoint

- Timestamp: 2026-08-28T10:57:54Z
- Exact HEAD/base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Worktree/branch: `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`, `worker/display-d2`
- Status: waiting/not live; serial compiler lane released to the manager

Terminal executable evidence:

- strict Debug configure exit 0, focused build terminal, Display-service CTest
  3/3 pass;
- strict Release configure exit 0, focused build terminal, the same CTest 3/3
  pass;
- ASan+UBSan focused build terminal, the same CTest 3/3 pass with
  leak/error-halting options;
- staged Release install contains exactly 28 product files: five Display
  libraries, 19 public headers, the resident executable, D-Bus activation
  descriptor, systemd user unit, and Display1 XML;
- all four staged D2 public headers compile as first includes, and the linked
  installed-surface consumer exercises D0 JSON decode plus D1 projection and
  exits 0.

The final source pass additionally makes callback lineage request-scoped (a
port copies the outer lineage with each apply request rather than retagging a
late callback) and rejects D0 text controls/format/NUL/over-limit values in the
decoder itself. All three build modes and tests above include those repairs.

One attempted `systemd-analyze verify --root=...` lookup exited 1 because that
invocation did not locate the staged user unit. It emitted no unit-content
diagnostic; the deployment unit already compiles/tests and is staged. The
remaining work is bounded to the corrected static lookup, strict MkDocs via
the documented Python environment (the bare command is absent), final
source/docs/whitespace/dependency/diff audit, immutable commit, and exact
different-worker review.

The product diff is preserved uncommitted at 27 paths, +2,776/−25. No D2
compiler process is live. No resident executable, nested/private/session bus,
display/input path, or host service was launched or touched.
