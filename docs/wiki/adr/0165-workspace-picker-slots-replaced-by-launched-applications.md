# ADR-0165: workspace picker slots replaced by launched applications

- **Status:** Accepted
- **Date:** 2026-09-15
- **Owners:** First-party applications (File Manager) with Platform
  (compositor workspace bridge)
- **Supersedes:** None
- **Superseded by:** None

## Context

A reopened saved layout must be able to reopen *places* instead of
applications: a slot may be occupied by a File Manager picker (the
Applications browser of [ADR-0164](0164-shared-application-catalog-and-file-manager-applications-browser.md))
showing the standard application hierarchy, and the application the user
picks replaces the picker, taking its exact place in the layout. The workspaces
contract forbids guessing window identity, the topology requires every leaf
to name a live member, and AdoptIndependentLayout demands fully bound
layouts — so the picker must be a real window bound like any other member,
and its replacement must be an atomic topology transaction, not a detach
plus a dock.

Alternatives considered: making the picker a pure compositor surface (duplicates
the whole application hierarchy outside the file manager and breaks the
"addition to the QindaQt File Manager" requirement); swapping by D-Bus caller
identity (a client has no compositor window identity to name); keeping the
picker alive inside the layout after the replacement (two windows for one
application).

## Decision

1. **Binding.** The reopen dialog's per-slot *Picker instead* toggle launches a
   File Manager picker window (the ordinary installed desktop entry). The
   picker is then assigned to the slot like any other window — the existing
   explicit assignment plan stays authoritative and nothing is guessed.
2. **Arming.** `WorkspaceUiPort::restore()` receives the slot→picker-window
   bindings (`pickerSlotWindows`). After the atomic adoption commits, the
   compositor registers each binding as a pending replacement keyed by the
   picker member's window id, carrying the slot's saved desktop-entry id.
3. **Choosing.** The picker's activation calls the new
   `org.qindaqt.Compositor1.ChooseApplicationForActivePicker` control-endpoint
   method with the entry id only. The compositor resolves the call against
   the **active window**, which must be a registered picker: a caller can
   never name another window's placeholder, and the picker never needs its
   own compositor identity. The compositor launches the chosen entry through
   its desktop-application adapter (full facility: terminal and D-Bus
   activation), with a bounded arrival window.
4. **Replacing.** When a normal, independent window whose
   desktopFileName-then-resourceClass identity matches a launched pending
   entry arrives, the new atomic `ReplaceMemberWindow` topology command
   rebinds the picker's leaf to the arriving window — leaf node id, page
   membership, and all ratios preserved — returns the picker to the
   independent set, and closes it. The arriving window takes the picker's
   exact place in the layout.
5. **Failure is bounded and visible.** A pending replacement expires (a
   picker left unchosen expires too); a picker closed by its user drops the
   pending; a failed launch leaves the picker usable; a replacement that
   never arrives simply expires with the slot still showing the picker. The
   chooser reply reports launch failure synchronously.

## Consequences

- The picker route enables terminal and D-Bus-activatable entries the File
  Manager's standalone launch path deliberately rejects (ADR-0164); the
  compositor owns the full desktop-entry launch facility.
- `Core::WindowContainer::replaceWindow` is a new pure-model mutation: an
  in-place leaf rebind that cannot disturb split topology, node ids, or
  ratios, with duplicate-membership rejection. `ReplaceMemberWindow` is its
  typed topology command with one publish-or-nothing transaction.
- The chooser route is production-enabled (unlike the development mutation
  gate) because its validation is the active-window-is-a-registered-picker
  rule; there is no free-form mutation surface.
- Window-arrival correlation uses the same identity rule as the workspaces
  assignment policy; a second window with the same identity binds the first
  eligible arrival and the pending is consumed.

## Revisit when

- Pickers need to survive compositor restarts (fold pending state into the
  future session-restore format).
- A second picker consumer appears (generalize the chooser handler beyond the
  active-window rule behind the same endpoint).
