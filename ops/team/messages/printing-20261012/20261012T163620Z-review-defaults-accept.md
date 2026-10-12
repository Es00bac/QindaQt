# ACCEPT: printing defaults candidate 39d26063991f9713133f0661ff5db3fd2b2a2ab1

- Timestamp: 2026-10-12T16:36:20+00:00
- Reviewer: printing-review-20261012
- Candidate: `39d26063991f9713133f0661ff5db3fd2b2a2ab1`
- Base: `2dadb4e833afa612d1889495505ae0d57585c020`
- Verdict: ACCEPT for source/package integration. No blocking finding in the reviewed delta.

## Reviewed contracts

The package applies only its own target preset on first install, honors earlier administrator policy and masks, suppresses live operations for image roots/unbooted systems, and does not repeat enable/start on upgrades. The target uses ordinary systemd Wants dependencies; provider masks remain authoritative. Standalone provider disable is correctly documented as insufficient to prohibit activation. Existing CUPS queues/configuration are not modified; an already running CUPS scheduler receives an explicit safe-restart notice instead of an automatic interruption.

Dependency floors include the corrected SpliX 2.0.2 and cups-browsed 2.1.1-r1; driverless USB is independently packaged from the exact upstream 0.9.34 archive. I inspected the vendored sole Go dependency and compatible BSD-2 notice, offline build flags, original limited USB-interface matches, loopback HTTP default, protected /etc configuration and corrected installed ExecStart path. No printer-sharing configuration is enabled by this delta. Upstream udev source: https://github.com/OpenPrinting/ipp-usb/blob/0.9.34/systemd-udev/71-ipp-usb.rules

## Evidence

- Independently ran `python3 -m unittest discover -s tests -p test_printing_defaults.py -v`: exit 0, 10/10 tests.
- Independently verified archived byte count, BLAKE2B and SHA512 against candidate Manifest: passed.
- Inspected final exact-floor pretend log: 8 packages, including SpliX 2.0.2; no desktop/Office rebuild.
- Inspected author Portage ipp-usb successful test/image log: 17 PASS records and completed test/install phases. Author reports exit 0, 14 files.
- Inspected defaults image log and exact staged unit check: completed install; systemd verification records exit 0. Author reports defaults image exit 0, 2 files.
- Author reports syntax/XML/Manifest/diff checks exit 0; candidate tree was clean at review start and no source changes were requested.

## Bounded caveats and next action

Manager may integrate and adopt the package through Portage, then run installed dependency/service acceptance. The review does not certify a physical sheet, network printer or scanner, and does not establish the root cause of the observed Samsung no-page jobs. Existing admin configuration remains out of scope. The harmless pre-existing /etc/gitconfig write warning in the package image log did not stop the completed phase.

Awaiting the exact Office candidate; offering immediate focused rereview if package integration exposes a concrete defect.
