# QindaQt Settings — Printers & scanners

`qindaqt-settings --page printers-scanners` provides printer setup/jobs and
document scanning through existing installed applications. It is route 24 in
[Settings Center](settings-center.md); the previous 23 indices and digit
shortcuts stay unchanged. Ctrl+K searches terms such as printer, CUPS and scanner.

The source implements the page and focused fixtures. Native, relocated package,
Portage dependency and physical journey gates must be recorded separately;
this source description is not a successful print or scan receipt.
[ADR-0356](../adr/0356-delegate-printer-and-scanner-settings-to-installed-tools.md)
records the delegation and lifetime contract.

The [printing stack packaging decision](../adr/0376-ship-a-complete-printing-stack.md)
defines the separately delivered scheduler, filters, discovery, drivers and
graphical setup dependencies. A desktop entry alone does not install or enable
that stack. On October 12 the attached Samsung ML-216x was visible over USB but
the laptop had no configured queue, enabled CUPS socket, conversion filters,
Samsung driver or graphical setup tool. Delivery and physical print evidence
are tracked separately from the Settings page's source tests.

The QindaGentoo desktop profile now includes `net-print/qindaqt-printing-1-r1`.
Its reviewed package supplies CUPS with USB/DNS-SD support, cups-filters,
cups-browsed, ipp-usb, Gutenprint, SpliX and system-config-printer with polkit.
The package's systemd target starts the providers on a fresh install, preserving
administrator presets and service masks. Existing CUPS queues and configuration
remain untouched; updates do not re-enable or restart the target. If CUPS was
running before its filters were installed, finish pending jobs and restart the
scheduler once to load those filters.

## Samsung USB qualification, October 12

Portage installed SpliX, cups-filters and system-config-printer with their
dependencies. Enabling the installed CUPS socket and replaying the attached
printer's USB add event invoked the upstream `configure-printer@` service.
It automatically selected `samsung/ml2160.ppd.gz`, created `ML-2160-Series`
and made it the default. Qt's CUPS plugin enumerated the same default with
23 paper sizes. No machine-specific URI or serial is stored here.

Because CUPS had started before the filters were installed, its first test
submission rejected the stale MIME database. Reloading the scheduler made
the new conversions available. Subsequent job 2 reported
`job-completed-successfully` with one sheet and the printer returned to idle.
Held job 3 was canceled through the ordinary user's CUPS connection, moving
from held to canceled with zero sheets. These are backend receipts; readable
paper confirmation and the repaired Office path are separate gates. The owner
reported that job 2 had no paper loaded and job 4 still produced no page after
loading paper. After a physical disconnect/reconnect, short text job 5 again
reported one sheet through CUPS; physical confirmation remains pending. The
device's live USB identity response then reported `STATUS:IDLE`, and its basic
status flags did not report paper empty or error. None of these software
receipts establishes successful physical output.

The owner subsequently found the paper tray was overfilled, removed excess
paper, printed the device's own configuration page and reported a green light.
With SpliX 2.0.2 and the refreshed ML-2160 PPD, production QindaWrite fixture
job 8 then completed one sheet and the owner replied “Got it” after submission.
This qualifies one real page from the Write renderer through Qt, CUPS and USB.
The no-reattach diagnostic used only job 6; no USB quirk override remains on
the queue, and the successful Write job used ordinary settings.

The laptop now has printing defaults 1-r1 and Office `0.1.0_p20261010-r3`
installed through Portage. The printing target is enabled and its CUPS,
discovery and Avahi services are active. The temporary independently enabled
CUPS socket was retired; the printing target owns its boot dependency. No
compositor, shell or open document was restarted. Save and reopen an already
running Office application to load the updated executable.

Office source `d47c4d905534bea0b2d65394eebf35dc5b7bbab5` adds explicit printer
refresh, refusal of a vanished selection, completion/error handling and
persistent error presentation. Write honors page ranges and the first selected
section's paper. Independent and manager owning CTests pass 2/2; four narrow/
wide dialog captures were inspected, and the opt-in hardware fixture passed.
The signed r3 artifact's 68 installed files match its ledger; installed
QindaWrite's isolated startup/help check passes. Other Office renderers' page
range behavior, other printer models, network devices and scanning retain
their separate qualification boundaries.

Actual first installation exposed EAPI 8's empty `EROOT` convention: version 1
enabled the target but skipped its immediate start. Reviewed overlay repair
`422c0248519c47d225a716c52864bf7a0f64e20c` normalizes the real root and excludes
Prefix/image roots. Twelve policy cases pass, and the new regression fails
against the original recipe. Upgrades continue to preserve deliberate disables.

## Sharing a USB printer over the network

A USB printer can stay attached to one desktop while other machines print
through that desktop's CUPS server. Install the printing stack on the server
through Portage, enable sharing with `cupsctl --share-printers`, and mark only
the intended queue shared with
`lpadmin -p QUEUE -o printer-is-shared=true`. This follows the
[OpenPrinting sharing procedure](https://openprinting.github.io/cups/doc/sharing.html).
The normal sharing option admits the local subnet; it does not enable remote
administration or unrestricted access from other networks.

The server owns the physical USB driver and filters. Clients use its IPP queue
with an IPP Everywhere configuration, for example
`lpadmin -p QUEUE -E -v ipp://SERVER.local:631/printers/QUEUE -m everywhere`.
Mark that client queue unshared to avoid advertising a second proxy printer.
When moving a printer from a client's USB port to the server, update the
existing queue's URI and driver and keep its name/default, so applications'
saved selections continue to resolve. Re-enable the client queue if unplugging
the USB cable had disabled it. Do not equate successful submission with paper
output: check both client and server job state and confirm a physical page.

### October 12 network qualification

The owner moved the Samsung to the desktop host. Portage installed printing
1-r1 and its ten missing/reconfigured dependencies there, preserving the
existing DNS-SD compatibility flag. Its printing target is enabled and CUPS,
cups-browsed and Avahi are active. The physical queue uses SpliX's ML-2160
PPD and is shared, with local-subnet access and remote administration disabled.

The laptop's existing `ML-2160-Series` default now uses the server's IPP queue
and an IPP Everywhere driver; its disconnected USB backend is gone. The client
queue is not shared. DNS-SD advertises the physical server queue to other
network clients, and automatic discovery remains enabled. Machine-specific
addresses and device serials remain administrator configuration rather than
package defaults.

The actual production Write print-dialog fixture passed 3/3 Qt methods with
live discovery, and its hardware fixture passed 3/3 methods. Laptop job 11 and
server job 1 have the same job UUID, each reports one sheet and
`job-completed-successfully`, and both queues returned to idle. This establishes
the network submission/driver path. The owner confirmed that the page printed
correctly, completing the laptop-to-desktop-to-Samsung physical check. No model
workload or inference service was started for this verification.

## Existing owners

| Page action | Fixed catalog identity | Owning operation |
| --- | --- | --- |
| Open Print Settings | `Gentoo-system-config-printer`, then portable `system-config-printer` when the first is absent | Existing OpenPrinting UI owns adding/configuring printers, test pages and job view/cancellation. |
| Open CUPS Administration | `cups` | The installed desktop entry opens the CUPS web interface; server availability and authentication stay there. |
| Open Document Scanner | `org.gnome.SimpleScan` | The selected packaged SANE frontend owns device/format/destination selection, PDF/image output and scan errors. |

The [OpenPrinting project](https://github.com/OpenPrinting/system-config-printer)
owns the printer administration UI. [GNOME Document Scanner](https://apps.gnome.org/SimpleScan/)
provides SANE scanning with PDF/image export. Install their supported Gentoo
packages through Portage; this page executes no package or privilege command.
Document Scanner is `media-gfx/simple-scan` (GPL-3+); Print Settings is
`app-admin/system-config-printer` (GPL-2+), with existing CUPS/driver dependencies.

Native application printing remains with [Print/PreparePrint](../reference/portal-misc-families.md)
and [Text Editor printing](text-editor.md#printing). Viewer printing is a
separate consumer; this page does not spool files, convert PDFs or mint tokens.

## Availability, launch and retry truth

The page always exists. Each action reports whether its fixed installed menu
entry can be planned; a missing, masked, invalid or unsupported entry disables
that action and shows a plain diagnostic. Missing discovery means the tool was
not found in the bounded public catalog, not an authoritative package query.
It does not assert printer service or device health.

Refresh installed tools rereads metadata without launching anything. After
installing or repairing a tool, refresh enables its action when the catalog
admits it. A click reads and plans again, so an entry removed or replaced after
the last refresh cannot reuse a cached process plan. Higher-root Hidden/malformed
entries mask lower copies of the same identity. A terminal or D-Bus-only entry
is unsupported here; an unsupported primary printer entry does not fall through.

Only a validated literal process argv reaches the injected starter. The page
accepts closed action IDs and receives no program/path/URL/argument fields.
Successful startup reports a launch request, leaving connection/device/job
truth in the application. Failed startup reports retry; refresh never replays
the previous click. There is no passive service activation, automatic mount,
printer setup, scan, authentication or system configuration write.

## Composition and public boundary

The `src/apps/settings/printing` module owns three small collaborators:

- `PrintingSettingsModel` projects three fixed rows, action admission and
  submission/refusal text. Borrowed catalog/starter ports outlive it and run on
  the same GUI thread. There is no persistence, asynchronous replay or binary
  compatibility promise for these process-local values.
- `ApplicationPrintingToolCatalog` owns copies of explicitly ordered XDG data
  roots and consumes the public ApplicationCatalog scan/launch-plan APIs.
  Parsing, precedence and discovery bounds stay in that public producer.
- Engine-local `PrintingRouteComposition` owns the catalog, literal QProcess
  starter and model in dependency order. Only the composition resolves XDG
  roots; QML never reads the environment or executes commands.

The shared compiled `QindaQt.SettingsApp.Printing` module belongs to the
Settings runtime install component. The executable requires its directory in
its own resolved prefix before QML construction. It does not borrow a missing
module from the developer tree or a globally installed desktop copy.

## Keyboard and responsive page

The real page uses QST-1 and public QindaQt.Controls. Labels, owner names and
diagnostics use plain text. The fixed actions and refresh form a Tab/Backtab
cycle that skips unavailable actions; the first admitted action is the page
focus target, or refresh when all tools are absent. Space and Enter submit one
deliberate intent. Focus reveals the action in the bounded scroll viewport;
PageUp/PageDown scroll the form. The same page fits compact 420×320 and desktop
layouts, including logical geometry at DPI 2. Settings owns route entry/return,
Alt+Left navigation and the Ctrl+K palette.

## Verification and delivery gates

Focused source gates are `qindaqt.settings-printing-model`,
`settings-printing-catalog`, `settings-printing-page`,
`settings-printing-page-dpi2` and `settings-printing-installed-route`.
They cover inert inspection, closed IDs, stale entry loss/replacement,
masking/unsupported entries, literal arguments, missing/install/retry,
single-flight dispatch, real keyboard/layout and plain labels. The installed
fixture relocates only the Settings runtime component, isolates XDG roots and
both buses, requires the exact active Loader Ready witness, withholds Printing
while the developer module remains, checks failure, restores and checks Ready.

The existing registry/controller/search, real command palette, layout,
interaction, hostile startup, all-route active witness and installed Settings
gates remain required. The construction expectation is 24; the unrelated fake
witness negative still explicitly uses its own two-route inventory.

Separately qualify the Portage-installed tool/dependency entry and actual
printer/scanner workflows: printer discovery/addition, test page, failed job
view/cancel; scanner choice/format/destination, PDF/image save and unplug.
Fixtures and catalog metadata do not establish those hardware journeys.
