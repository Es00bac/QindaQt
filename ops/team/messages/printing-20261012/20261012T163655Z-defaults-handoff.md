# Printing defaults handoff

- Time: 2026-10-12T16:36:55+00:00
- Exact candidate: 39d26063991f9713133f0661ff5db3fd2b2a2ab1
- Branch: fix/printing-defaults-20261012, pushed to qinda QindaGentoo hub
- Worktree: ~/work_space/container-wm/.cache/printing-overlay-20261012
- Status: idle; independent exact review is in progress with printing_review.

## Changed paths

- README.md
- metadata/qinda-delivery
- profiles/categories
- profiles/qindaqt/systemd/packages
- profiles/qindaqt/systemd/package.use
- profiles/qindaqt/systemd/package.accept_keywords
- net-print/ipp-usb/Manifest
- net-print/ipp-usb/ipp-usb-0.9.34.ebuild
- net-print/ipp-usb/metadata.xml
- net-print/qindaqt-printing/qindaqt-printing-1.ebuild
- net-print/qindaqt-printing/metadata.xml
- net-print/qindaqt-printing/files/qindaqt-printing.target
- net-print/qindaqt-printing/files/50-qindaqt-printing.preset
- tests/test_printing_defaults.py

## Acceptance evidence

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_printing_defaults.py -v`: exit 0, 10/10 policy cases. Includes real systemctl preset operations only in disposable roots; mocked live startup; masks, administrator presets, config/queue preservation and upgrade no-restart behavior.
- `bash -n` for both new recipes: exit 0. Both metadata XML parses: exit 0. `git diff --check`: exit 0.
- `ebuild ... manifest` for each new package: exit 0. ipp-usb archive Manifest pins 128786 bytes; the source-free defaults package correctly has no thin Manifest entries.
- Exact candidate isolated-profile `emerge --pretend --verbose --oneshot --getbinpkg=n --autounmask=n --backtrack=30 '=net-print/qindaqt-printing-1'`: exit 0, 8 packages (Splix 2.0.2 upgrade, 4 new, 3 rebuilds), no desktop/Office rebuild. Log `/tmp/qindaqt-printing-build/logs/printing-candidate-pretend.log`.
- Portage ipp-usb source compilation: exit 0. `sudo ... ebuild net-print/ipp-usb/ipp-usb-0.9.34.ebuild test install`: exit 0, 17/17 upstream tests and 14 staged files. Log `/tmp/qindaqt-printing-build/logs/ipp-usb-test-image2.log`.
- Defaults Portage `ebuild ... clean install`: exit 0, 2 staged files. Log `/tmp/qindaqt-printing-build/logs/qindaqt-printing-image.log`.
- `systemd-analyze --root=/tmp/qindaqt-printing-build/verify-root --man=no --generators=no --recursive-errors=no verify /usr/lib/systemd/system/qindaqt-printing.target /usr/lib/systemd/system/ipp-usb.service`: exit 0 for exact staged units with external-provider fixtures. Log `/tmp/qindaqt-printing-build/logs/unit-verification.log`.
- Actual host MAKEOPTS before/after remains `-j32 -l16`. No worker host merge, printer action or service activation occurred.

Initial non-root image ownership and subsequent root/portage Go cache ownership failures were corrected only in the temporary build tree before the passing image/test gate. The signed distribution binary and final installed runtime are manager gates, not claims of these stage checks.

## Remaining bounded caveats and next action

- Existing running CUPS can cache the pre-install MIME/filter database. First-install notice and README direct an operator to let jobs finish before one restart; upgrades never silently restart it.
- Splix >=2.0.2 brings the upstream missing-page cache-race repair, but no physical Samsung fix is claimed. Physical readable output remains manager-owned.
- IPP-over-USB hardware and discovered network-printer runtime are not physically qualified here; no such device was attached to this worker.
- Request: complete exact independent review, integrate and adopt through Portage, then perform manager-owned physical/application gates.
- Help offer after reading current queues/peer thread: inspect an exact package-adoption failure or repair a reviewer reproduction in this same worktree. No unrelated queue work or hardware action claimed.
