# Saved workspaces

A workspace keeps a container's name, chosen color, split layout, tabs and
application choices so that related work can be assembled again after login.
It describes places for windows, rather than remembering window handles from a
session that has ended. Two terminals may have different jobs; restoring a
workspace must not guess which is which.

## Current implementation boundary

`QindaQt::Workspaces` provides the saved document, atomic filesystem storage,
capture of an existing layout, window assignment policy and construction of a
fresh container value. `QindaQt::WorkspacesUi` provides compact native Qt
Widgets Save and Reopen dialogs over those public operations.
`KWinWorkspaceUiPort` is the compositor-side bridge: it snapshots a
preselected live container, discovers installed desktop entries, dispatches
launches, and sends a fully assigned layout through the runtime's atomic
adoption command. Session composition remains responsible for constructing the
bridge and connecting its signals, so this module does not itself prove an
installed logout/restore session.

The public boundary is `qindaqt/workspaces/workspace.h` and
`qindaqt/workspaces/workspace_store.h`. Values own their data and can cross
threads by copy. `WorkspaceStore` is synchronous; its caller supplies the
storage directory and serializes access on the calling thread. The native
dialog performs its small document reads and atomic writes synchronously on
the GUI thread. Saving uses atomic
replacement. Failed validation leaves the previous document intact; loading a
damaged or unsupported document reports an error without rewriting it.

## Names, colors and application slots

Version 1 stores a workspace identifier, a one-line name, an optional sRGB
`#RRGGBB` accent, a Core container layout and application slots. An empty color
means follow the theme. Each slot has a durable identifier, a human label, a
desktop-entry identifier and optional absolute URLs. Layout leaves refer to
slot identifiers. The sets must match exactly. A saved layout contains at
least two slots, consistent with published container ownership.

Window matching receives an explicit inventory with eligibility supplied by
the compositor. It never chooses an ineligible window. Automatic matching is
allowed only when an application has exactly one remaining slot and exactly
one eligible window. Duplicate terminals or editors require a manual choice.
An explicit choice may select a replacement application. A window cannot
occupy two slots.

Instantiation requires complete assignments and produces a new Core container
value with fresh live window identities while retaining page order, active
page, split orientation and ratios. It does not mutate the session. The
compositor must revalidate live eligibility and publish adoption atomically.

## Native Save and Reopen dialogs

`WorkspaceLibraryDialog(QString storageRoot, WorkspaceUiPort &, QWidget *)`
owns only GUI-thread dialog state. Its caller explicitly supplies the durable
storage root and keeps the borrowed `WorkspaceUiPort` alive for the dialog's
lifetime. Every port call is synchronous on the GUI thread; it returns owned
snapshots and never exposes a KWin or application-process object to the UI.
Save, Reopen, and color-picker dialogs are heap children opened asynchronously
with `WA_DeleteOnClose`; the library therefore owns and deletes them before its
borrowed port can be torn down. Their accepted callbacks use the library as
their QObject context, so no delayed save or restore callback can run after
library destruction.

The Save action asks the port for one complete current-container snapshot,
prefills its name and color, then calls `capture()` and `WorkspaceStore::save()`.
An optional saved-workspace ID in that snapshot intentionally replaces the
matching document when saving an existing template. Cancelling capture or a
validation/store failure leaves the previous document untouched.

The name dialog uses the native `QColorDialog` rather than a color-code entry
field. **Choose color** records its selected sRGB value as canonical
`#RRGGBB`; **Follow theme** clears it. The library shows saved accents as small
swatch icons, leaving workspace text on the native palette for contrast. It
keeps valid rows visible if another workspace file is damaged and reports one
concise read-error notice.

The Reopen action lists every saved slot with a title-bearing eligible-window
choice. It feeds both the inventory and every explicit choice to
`assignWindows()`; it never reimplements automatic matching. Refresh obtains a
new inventory, preserves only still-eligible explicit choices, and recomputes
the plan. Thus duplicate Terminal slots remain visibly unassigned until the
user selects different windows, and a missing desktop entry plainly asks the
user to choose a replacement window. A per-slot **Launch application** button
is enabled only for an installed entry and reports dispatch only: the user
chooses **Refresh available windows** when its window appears.
Installed application display names appear in slot and eligible-window labels;
desktop-entry IDs remain available as diagnostic tooltips and are shown when an
application is unavailable.
If the later asynchronous launch completion reports failure, the adapter queues
`WorkspaceLibraryDialog::reportLaunchFailure()` onto the GUI thread. The method
keeps the Reopen dialog and its explicit choices open while showing the error.

Restore remains disabled until `AssignmentPlan::complete()` succeeds. The UI
then calls `instantiate()` and hands both the original `Workspace` (including
name/color identity) and bound fresh layout to `WorkspaceUiPort::restore()`.
The adapter must atomically revalidate the now-live inventory and reject stale
choices. A rejection remains in the dialog with its choices intact and never
rewrites the saved document.

The port consists of `currentContainer()`, `availableWindows()`,
`installedApplication()`, `launchApplication()`, and `restore()`. Its public
values are `CurrentContainer`, `WorkspaceWindow`, and `InstalledApplication`;
see `qindaqt/workspaces_ui/workspace_dialogs.h` for their exact signatures and
lifetime contract. Platform composition belongs outside this module.

## Verification and remaining work

`workspaces.persistence-assignment` verifies persistence across store lifetimes,
last-save preservation, damaged-file reporting, schema/layout rejection,
identity rebinding, ambiguous application matches, explicit replacement and
ineligible/duplicate-window rejection. These tests do not prove logout restore.

Capture accepts a complete application-intent inventory for the current
container, preserves page order and active page, and replaces live leaf IDs
with durable slot IDs. A stale or incomplete inventory rejects the capture.

`workspaces-ui.dialog-actions` clicks through save, duplicate-window selection,
and a failed atomic restore while preserving the saved document.
Container name/color projection and roll-up/iconify controls are implemented
separately at the compositor boundary. Their geometry and visibility must not
be confused with the durable split layout.

See [ADR-0097](../adr/0097-separate-workspace-slots-from-live-windows.md),
[Window containers](window-containers.md) and
[Module boundaries](module-boundaries.md).

## Desktop application adapter

`QindaQt::WorkspacesApps` supplies installed desktop-entry lookup and
asynchronous launch through KF6 Service and KIOGui. Missing applications return
a useful error immediately; failed process startup arrives through
`launchFinished`. The caller must display that failure and refresh the live
window inventory separately. A successful launch is not a window assignment.
The adapter accepts a caller-supplied Wayland activation token and keeps URL
expansion in the platform launcher. See
[ADR-0101](../adr/0101-launch-workspace-apps-through-desktop-entries.md).

`workspaces.desktop-applications` runs on a private test bus with a temporary
XDG application catalog. It exercises missing-app rejection, real desktop-file
launch with a spaced document URL, asynchronous executable failure, and invalid
URL rejection. It does not launch the user's installed applications or prove
host-session window adoption.

The Hybrid `AdoptIndependentLayout` command provides atomic model/scene
adoption for a fully bound layout. `KWinWorkspaceUiPort` rejects stale,
non-normal, or already-grouped windows before it calls that command exactly
once. It first validates the bound Core layout before enumerating its members,
then validates name/color presentation before adoption; if the later
presentation write fails, its warning explicitly says the layout was restored.
See [Hybrid topology](hybrid-topology.md).
