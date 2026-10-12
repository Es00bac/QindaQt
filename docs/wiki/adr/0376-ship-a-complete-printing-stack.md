# ADR-0376: Ship a complete printing stack

- **Status:** Accepted
- **Date:** 2026-10-12
- **Owners:** Platform packaging and first-party applications
- **Supersedes:** None
- **Superseded by:** None

## Context

The owner connected a Samsung ML-216x USB laser printer but could not print
from QindaWrite. The laptop had CUPS and Qt's CUPS plugin, yet its scheduler
and socket were disabled, no printer queue existed, and the raster filters,
Samsung driver and graphical setup tool were absent. A usable
[Printers & scanners](../apps/printers-scanners-settings.md) page cannot make
missing system dependencies available.

[ADR-0356](0356-delegate-printer-and-scanner-settings-to-installed-tools.md)
delegates printer administration to existing tools. This decision supplies
their package and activation contract without adding a QindaQt print daemon.

## Decision

1. The QindaGentoo profile includes a dedicated `net-print/qindaqt-printing`
   component. Portage owns its dependency closure and systemd defaults:
   CUPS, document/raster conversion, IPP/DNS-SD discovery, driverless USB,
   common legacy printer drivers including SpliX, and the OpenPrinting
   graphical printer/job manager with its polkit integration.
2. First installation activates the package-owned printing target. Existing
   service masks and earlier administrator presets remain authoritative. Updates
   preserve the target's enable choice, CUPS configuration, queues and selected
   defaults. Disabling the target opts out of the aggregate boot policy;
   masking a provider prohibits it even when another unit wants it. CUPS retains its
   local socket/loopback administration boundary; the package does not enable
   printer sharing or legacy network browse listeners.
   If CUPS was already running when new filters were installed, finish its jobs
   and restart it once to load the new conversion/MIME database. Installation
   does not interrupt existing jobs with an automatic scheduler restart.
3. USB queue setup uses the packaged upstream discovery/driver matching path.
   Machine-specific printer URIs and serial numbers belong in CUPS runtime
   configuration, never in the desktop repository or profile. A user can choose
   or repair a device through Settings → Printers & scanners → Print Settings.
4. Applications retain rendering and print-dialog ownership. They discover
   printers through Qt/CUPS, refuse a vanished selected destination, and report
   submission failures. A successful spool submission is not evidence of
   printed paper. PDF export remains useful when a physical printer is absent.
5. Deploy the printing component independently of the compositor and shell.
   Any application repair is a separately reviewed Office package update.
   No desktop logout or compositor restart is part of printer setup.

## Consequences

A stock desktop has the scheduler, tools and drivers needed for ordinary
printing. The dependency footprint increases; exact model support still
depends on the installed upstream drivers. Modern IPP devices and legacy
Samsung SPL devices follow their respective upstream paths.

QindaGentoo candidate `39d26063991f9713133f0661ff5db3fd2b2a2ab1` is
independently accepted and integrated. It requires SpliX 2.0.2, packages
upstream ipp-usb 0.9.34 with vendored Go dependencies, and leaves CUPS's
local listener policy unchanged. Ten activation-policy fixtures, 17 upstream
ipp-usb tests, isolated package images and unit verification passed. The
installed dependency plan adds eight packages/upgrades/rebuilds on the
previously bootstrapped laptop, without a desktop rebuild.

This acceptance covers the package contract. The Samsung's physical output
remains unqualified after the owner reported no paper from jobs whose CUPS
transmission had completed; see the owning page's hardware record.

Acceptance includes the resolved Portage dependency plan, package image and
activation behavior, installed printer discovery, a known one-page job,
visible job state/cancellation, and application print-path checks. Physical
paper, other printer models, network devices and scanner hardware require
their own evidence; synthetic tests do not qualify them.

## Revisit when

CUPS removes the legacy filter/PPD path, distribution printer applications
replace those drivers, or real device evidence exposes missing defaults.
