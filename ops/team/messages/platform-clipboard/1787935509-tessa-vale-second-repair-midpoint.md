# Platform clipboard: second exact repair midpoint

- **Timestamp:** 2026-08-28T10:45:09-06:00
- **Worker:** Tessa Vale
- **Preserved parent:** `08d4352ceb2504f4ba337aec689a137352f4822c`
- **Status:** implementation and focused executable evidence complete;
  static/docs gates and commit remain

## Material repair

QCBV decode now performs two explicit phases. The first scans framing, media,
duplicates, per-format sizes, cumulative size, nonempty payload, and trailing
bytes while skipping payload extents without materializing them. Only a fully
accepted form enters a second pass that stages copied formats privately and
publishes the value after every read succeeds. Aggregate overflow, duplicates,
trailing bytes, and truncation therefore expose an empty public value.

Fixed-width format/list count reads in QCBV, QCBD, and QCDL are checked for
reader failure before zero is interpreted as semantic emptiness. Exact
`magic + version` QCBV/QCDL prefixes and a valid QCBD truncated immediately
before its format count now return `MalformedData`; the valid seven-byte empty
QCDL form remains accepted.

## Evidence so far

- Incremental strict Debug focused build — exit 0; all four Clipboard suites
  **4/4 passed**.
- Incremental strict Release focused build — exit 0; all four Clipboard suites
  **4/4 passed**.
- Exact regressions cover the 524,289 + 524,288 aggregate, empty result on
  overflow/duplicate/trailing/truncation failures, five-byte QCBV/QCDL, and
  pre-count QCBD truncation.

No host clipboard, bus, desktop, compositor, Wayland, input, or configuration
was contacted.
