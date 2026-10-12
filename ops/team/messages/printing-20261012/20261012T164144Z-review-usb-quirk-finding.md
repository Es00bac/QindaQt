# USB reattach experiment and exact quirk precedence

- Timestamp: 2026-10-12T16:41:44+00:00
- Reviewer: printing-review-20261012
- Scope: read-only upstream source; no hardware mutation.

CUPS v2.4.19 accepts the per-job `usb-no-reattach=true` option and uses it to suppress reattaching the usblp kernel driver after sending a job. A queue default can be set with the corresponding `usb-no-reattach-default=true` option, but a one-job trial avoids storing an unproven default. Restoring the prior physical binding may still require the manager's controlled reconnect. Source: https://raw.githubusercontent.com/OpenPrinting/cups/v2.4.19/backend/usb-libusb.c

The exact-model quirk wins over the vendor wildcard in `find_quirks()`. Therefore the ML-2160 `0x04e8 0x330f unidir` entry supersedes, rather than adds to, the Samsung-wide soft-reset entry. The installed file having both rows does not establish that this device receives a reset after every job.

The primary Fedora maintainer diagnostic recipe suggests the no-reattach experiment after unidir fails, but the ML-2160-specific report in comments 25/26 was resolved by unidir alone. Thus no-reattach is a supported discriminating experiment, not a known confirmed fix for this user's hardware: https://bugzilla.redhat.com/show_bug.cgi?id=873123

Reported these findings to root; root exclusively owns any submission, queue setting or reconnect. Exact Office freeze remains next review gate.
