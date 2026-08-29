# Gauss the 2nd — Virtual Desktop exact rereview midpoint

- Timestamp: 2026-08-28T14:02:08Z
- Exact candidate: `58f08ba8499b434e36b2746eff773bd29b2e6c45`
- Identity: detached clean HEAD; tree `ae540d84b2f57b767f8f4ace75234f58a626e44c`;
  sole parent `a1d8c6153f2398f057047331e505850f71143d08`

The dock/output repair is structurally correct. Every exact `scope=dock`
record reaches current/desired output comparison before mapped/committed
cardinality, so an uncommitted or mapped-false contradictory record also fails.
The two new one-valid-plus-one-phantom cases are non-vacuous and exercise both
current and desired identity fields.

## Material P2 finding

`tests/session/desktop_session_output.py:52-60` defines geometry as acceptable
only when every component is a Python `int`. That rejects legitimate non-bool
float representations such as `0.0`, `1920.0`, and `1080.0`, despite the
production Outputs inventory originating from `QRectF` at
`src/compositor/kwin/kwinoutputinventory.cpp:76-81` and the accepted repair
requiring non-boolean numeric int/float shapes. A source-safe exact-candidate
reproduction changed only one inventory's four geometry values to equivalent
floats; both Outputs and ShellVisibility cases failed with `the output is not
exact 1920x1080@1`. An integer `scale=1` remains accepted as intended.

The hostile test at `tests/session/test_desktop_session_output_unit.py:54-65`
checks boolean rejection but has no positive equivalent-float geometry row, so
61/61 units do not catch the regression. The bounded repair is to accept
non-boolean `(int, float)` geometry components while retaining exact value
comparison and add the missing positive row.

Current source-safe evidence: 61/61 units, 14 sources compiled in memory,
source-shape 998/998, docs/navigation 64, whitespace clean, and exact manifest
hash `fcdfa0abebdc27e14c53178486f182e32cbbd5b674c4b55ff38aed6bff88637f`.
Those passing gates do not waive the P2. I request repair by Noether as a new
non-amended descendant, followed by exact rereview; I am completing the
remaining call-site/document consistency audit before final verdict.
