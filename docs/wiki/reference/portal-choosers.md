# Native portal chooser contracts

The source selector routes FileChooser and AppChooser to QindaQt. All advertised
methods passed actual private frontend/native-input qualification; installed
desktop and sandbox delivery remain separate manager gates.
The process choice is in [ADR-0322](../adr/0322-native-portal-choosers.md), shared
authority in [Native portal foundation](../architecture/portal-foundation.md),
and selection in [Portal service](../architecture/portal-service.md).

## Primary wire

The installed xdg-desktop-portal 1.20.4 XML and frontend source were inspected:
[FileChooser backend XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.FileChooser.xml),
[AppChooser backend XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.AppChooser.xml),
[frontend FileChooser](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/src/file-chooser.c),
[frontend OpenURI](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/src/open-uri.c).

At `/org/freedesktop/portal/desktop`, FileChooser exports version 4:

| Method | Inputs | Delayed output |
| --- | --- | --- |
| OpenFile | `o,s,s,s,a{sv}`: handle, app, parent, title, options | `u,a{sv}` |
| SaveFile | same | same |
| SaveFiles | same | same |

AppChooser exports version 1:

| Method | Inputs | Output |
| --- | --- | --- |
| ChooseApplication | `o,s,s,as,a{sv}`: handle, app, parent, offered IDs, options | delayed `u,a{sv}` |
| UpdateChoices | `o,as`: live handle, replacement IDs | no values; named error on refusal |

Responses use 0 for validated selection, 1 for cancellation and 2 for failure.
Backend result data exists only on success. The 1.20.4 frontend adds `uris=[]`
to failed/cancelled FileChooser responses; this carries no selected path.
Only the actual current same-UID frontend unique
owner is accepted. The frontend supplies case-sensitive app IDs and owns caller
connection/Request lifetime.

## Selection and policy

File options enforce Boolean modal/multiple/directory, string accept label/name,
nul-terminated native path bytes for current folder/file, `a(sa(us))` filters,
`(sa(us))` current filter, `a(ssa(ss)s)` choices and `aay` multi-save names.
Unknown keys are ignored for forward compatibility. Paths are bounded absolute
literal names; suggested save basenames cannot traverse directories.

The GUI supports single/multiple files and directories, editable folder
navigation, plain labels, keyboard selection, MIME/glob filters and Boolean/
combobox choices. Open requires the requested existing file/directory kind.
Single save confirms replacing an existing regular file. Multi-save returns
one suggested path per name in order, using suffixes for collisions. No file
is created or truncated; selection cannot reserve a path against a later race.
Results contain canonical local `file://` URIs, exact offered choice values and
the selected filter when offered. Open sets writable false. Resident validation
precedes publication; standard document registration stays with the frontend.

AppChooser shows only public ApplicationCatalog IDs in the frontend's offered
set. Missing installed entries are omitted; no executable is substituted.
Updates rescan injected roots and replace candidates in the mapped dialog,
retaining selection only while offered. Success contains exactly `choice=s`.
Content-type, URI, basename and previous choice are bounded hints. The backend
does not launch or invent a version2 activation token.

Bounds: 128 selected files/apps/save names; 32 filters with 32 rules each and 32KiB
aggregate text; 16 choices with 32 options and 16KiB text; 4096 native path bytes;
512 title/label units. Helper request/update frames are at most 128KiB and output
at most 2MiB, with 128 updates and one live chooser child. Shared RequestRegistry
budgets remain unchanged. Missing/busy helper returns failure, never empty success.

## Lifetime and gates

The helper consumes an admitted ordinary FD, with no ambient display or KDE
chooser fallback. An empty parent is explicitly unparented. Nonempty parent
import must finish before enabling the dialog; invalid/lost parents fail.
Close, frontend/session loss, native uncertainty, deadline and teardown retire
the child and withdraw late output. All ports are same-thread and borrowed
dependencies outlive adaptors/processes.

`qindaqt.portal-chooser-policy`, `portal-chooser-requests` and
`portal-native-choosers` are the focused gates. The native journey uses the actual
frontend, production ordinary compositor and unchanged production dialog
sources with a separately linked test-only widget input driver. It uses only
temporary private files/app entries. Its matrix includes all file methods,
overwrite cancellation, filters/choices, actual OpenURI candidates and live
updates, Close, caller/frontend/supervisor loss and foreign-parent validity/loss.
The same gate passed nine QtTest cases without failures or skips. Source and
disposable staged routing proofs also require native response 2 with a zero KDE
chooser counter when no session was selected. These gates do not qualify an
installed physical desktop or a Flatpak document-permission journey.

The private native fixture waits for the native lock interface to be published
and then obtains the normal nonce-authenticated unlocked receipt before
constructing and attaching its ordinary portal composition. An independently
attached exact compositor owner/PID admits that observation; introspection and
properties do not grant unlocked truth. This fixture readiness fence preserves
the production content/identity/FD/result checks. Production startup retry
classification in the lock transport is separately owned.

## Removable devices

The file chooser's request-local sidebar consumes the same public
[MediaSource](../architecture/removable-media-client.md) as
[File Manager](../apps/file-manager.md#removable-devices-and-location-lifetime).
It shows separate attachment/partition rows with literal labels and deliberate
open, mount read-only, unmount, safely remove, details and graphical recovery.
Initial construction and observation perform no media action. An unmounted
open waits for the admitted result and confirming current roots, owner, epoch
and attachment before replacing the displayed folder.

Selected device folders retain attachment provenance through descendants using
the most-specific unique mounted root. Nested overmount and ambiguous equal
roots cannot silently preserve an old selection.
Attachment/root/owner loss clears selection and the filename, disables acceptance
and shows a visible choose-another-folder message. Later inventory at the same
pathname does not revive the old selection. Deliberate navigation to a current
device or another folder establishes fresh provenance. Known read-only media
remain browsable for OpenFile but disable both SaveFile and SaveFiles;
unknown read-only truth does not itself grant filesystem write authority.
Existing URI normalization, overwrite confirmation and frontend validation
continue to own final selection policy. Overwrite confirmation runs a nested
event loop: acceptance rechecks current media and the captured
selection/navigation generation after that prompt and before publication.

Closing/cancelling the dialog, frontend/parent loss and deadline withdraw
deferred navigation interest. A late mount reply cannot navigate the closed
request; accepted owner work continues. Formatting, credentials, preferences
and forced removal remain exclusively in the Removable Media app. The new
qindaqt.portal-media-chooser source fixture gate covers Open/Save/SaveMany,
read-only refusal, duplicate partitions, deferred open/close, replacement at a
reused root and explicit navigation withdrawal. Existing actual-native
frontend/foreign-parent qualification must be rerun on the integrated source;
fixture evidence does not replace installed-session or physical-USB evidence.
