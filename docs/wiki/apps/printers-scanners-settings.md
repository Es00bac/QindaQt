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
