# ADR-0356: Delegate printer and scanner Settings to installed tools

- **Status:** Accepted
- **Date:** 2026-10-07
- **Owners:** First-party Settings Center
- **Supersedes:** None
- **Superseded by:** None

## Context

[ED-11](../development/everyday-desktop-plan.md) requires discoverable printer
setup/jobs and a supported scanner workflow. Existing CUPS, Print Settings,
native Print and packaged scan applications already own these operations.
Building another printer/scanner engine would duplicate device, authentication,
driver, job and output policy. Installed application metadata does not establish
that CUPS is running, a device is present, or a job succeeded.

[ADR-0048](0048-settings-center-navigation-and-route-ownership.md) keeps route
authority local and typed. [ADR-0164](0164-shared-application-catalog-and-file-manager-applications-browser.md)
provides bounded installed application discovery and pure launch planning.
The owning page is [Printers & scanners](../apps/printers-scanners-settings.md).

## Decision

1. Append `printers-scanners` as route 24, preserving all prior 23 indices and
   digit shortcuts. Keep the route available even when no tool is installed:
   its page owns missing-tool diagnostics and explicit refresh. Search metadata
   includes printer, printing, CUPS, scanner and scanning.
2. Separate the route's presentation/model, public catalog adapter and process
   starter. Constructor-injected catalog and literal argv ports are borrowed,
   synchronous and GUI-thread confined; their owners outlive the model.
   Composition resolves XDG data roots once and owns those collaborators for
   the QML engine lifetime. There is no Settings1 persistence or service lookup.
3. Use only fixed public catalog identities: `Gentoo-system-config-printer`,
   its portable `system-config-printer` counterpart, `cups`, and
   `org.gnome.SimpleScan`. Prefer the Gentoo printer entry when present;
   an installed unsupported primary cannot fall through to a different entry.
   Public catalog precedence, Hidden masking, NoDisplay and validation remain
   authoritative per identity. No fuzzy name/executable/category discovery.
4. Refresh inspects metadata only. A deliberate click scans/plans again using
   exact retained public catalog bytes, so an inventory-time argv is not launch
   authority after entry removal or replacement. Accept ProcessSpawn plans
   only, bounded to a 4096-unit program/argument, 128 arguments and 65536 total
   code units. No terminal, D-Bus-only, raw command/path/URL or shell fallback.
   QML receives three closed presentation rows and never receives argv.
5. A start acknowledgement means launch submission only. Show a bounded plain
   refusal/retry or submission message; do not claim a window, print server,
   scanner, device or completed job. Reentrant dispatch is refused. Refresh
   never repeats an earlier launch. CUPS/driver/authentication/job and scan
   output/error handling stay in the existing tools; native application Print
   stays within its [existing boundary](../reference/portal-misc-families.md).
   This route never starts/enables a system service or installs a package.
6. Printing is an ordinary shared compiled Settings QML module, like
   Appearance/Display. Its directory joins the executable's required module
   preflight, with no static/developer/system fallback when relocated.
   Installed-only construction, withheld-module refusal and restoration are
   mandatory gates beside compact/normal/DPI 2 and keyboard tests.
7. GNOME Document Scanner through Portage's stable `media-gfx/simple-scan`
   is the selected document workflow. Its GPL-3+ SANE frontend supports PDF
   and image output; dependencies and installed/physical qualification remain
   a separately coordinated delivery gate. No source-built installation or
   alternate package manager is introduced.

## Consequences and qualification

The source implements one discoverable entry surface rather than another
device daemon. Missing/masked/invalid catalog entries produce disabled actions
and an explicit refresh without hiding the page. Valid catalog metadata makes
an action available; process startup can still fail and reports that separately.

The selected tools may require packages, running services, credentials and
drivers. A page fixture, relocated module or launch acknowledgement proves
none of those conditions. ED-11 closes only after the separately recorded
installed journeys and printer/scanner device cases, including failed job
cancellation and scanner unplug, are qualified.

Current source-shape review: the shared route adds two Main.qml bindings and
a supplemental Loader. Registry 484 and Settings main 473 nonblank stay below 500;
Main.qml 424 retains its existing QML shape debt while gaining no domain policy.
The host 343 nonblank remains below its 350 limit; its coupled single-loader,
focus and witness selection stays together under
[ADR-0250](0250-require-active-loader-witness-for-every-settings-route.md).
New printing production collaborators each remain below 275 nonblank.

Root independently accepted exact source `55113184ab9ca8b6a86eb074087aebb9cb6400a5` on 2026-10-08. Author strict production and focused/relocated gates pass 18/18 CTests (87 Qt checks); staged public catalog 1/1 (11 Qt checks), corrupt-header rejection and restoration pass. Manager integrated native rerun, package adoption and actual printer/scanner journeys remain open. The owner requested a pause for a fresh desktop login before those next gates.
