# QindaLink app

`qindaqt-link` (desktop entry `org.qindaqt.Link`, phone icon) is the QindaTK
companion app of [QindaLink](../architecture/qindalink.md). It is a client of
`qindaqt-link-service` and of `qindaqt-link-mirror`; it owns no phone state and
never runs `adb` itself. It looks and behaves like the
[System Monitor](system-monitor.md): QindaTK controls only (`import QindaTK as
Tk`), dense, dark, with the AppShell global menu.

Source: `src/apps/link`. Tests: `tests/apps/link` (`qindaqt.link-app-*`).

## Pages

| Page | What it shows | Service calls |
| --- | --- | --- |
| Devices | The paired phone (name, model, transport, battery, network, last seen, capabilities), pairing requests with the code as `123 456`, Unpair behind a confirmation, and **Set up phone over USB** | `Devices`, `PendingPairings` (tolerated if absent), `PairingRequested`, `ConfirmPairing`, `Unpair`, `Setup.*` |
| Messages | Conversation list with unread badges and search over the loaded threads; bubbles, MMS images, paging of older messages, compose and Send | `Messages.*`, `ThreadsChanged`, `SendStatus` |
| Phone apps | The phone's launchable apps, search, favourites, **Open on desktop** | `Setup.AdbDevices`, then `qindaqt-link-mirror --serial S --list-json` / `--package P` |
| Notifications | Per-app mute switches | `Notifications.ListApps`, `SetAppMuted` |
| Input & macros | Touchpad and keyboard status by capability (unavailable until the input feature is in the build); a button to Settings, Input, Macros. No macro editor here (ADR-0436) | none |
| Vault | "Vault typing is configured on the phone" | none |

## Rules that are easy to break

- **Nothing is sent without a click.** `ConversationViewModel` reaches
  `Messages.Send` only from the Send button, and the first send of a session
  also needs the OK in a dialog that lists the recipients. The text box never
  submits on Enter, and a draft is never restored into a send.
- **Pairing is a decision.** Showing a code never calls `ConfirmPairing`;
  Accept and Reject do. Reject takes focus so a stray Enter refuses.
- **Setup runs through the service.** The app calls `Link1.Setup.SetupPhone`;
  the service runs the `adb` grants. The app shows the per-step report and can
  re-read the state with `SetupStatus` without changing anything.
- **RCS is invisible.** An info strip says so once; dismissing it is stored in
  `$XDG_CONFIG_HOME/qindalink/app.json` as `rcsNoticeDismissed`.
- **Privacy.** Message bodies, addresses, snippets, notification text and reply
  text are never logged. The app's own files hold only favourites and the
  RCS-strip flag.

## Command line

```
qindaqt-link [--page devices|messages|apps|notifications|input|vault] [--size WxH]
qindaqt-link --reply -- <device> <key> <action>
qindaqt-link --fake populated|empty|unavailable|error [--page P] [--open-thread N] [--size WxH] --grab out.png [--grab-after ms]
```

- **Reply window.** `--reply -- <device> <key> <action>` is the argv the
  notification path launches (`ProcessReplyLauncher`); the `--` keeps dashed
  values from being read as options. The window is titled with the app name
  from `Notifications.Current()`, sends with `Notifications.Reply`, closes when
  `ReplyFinished` arrives without an error, and shows the error otherwise.
- **`--fake` and `--grab`.** A fake run uses scripted, invented data
  (`FakeLinkApi`), a throwaway preferences file and a recording process
  runner. It never touches the session bus, the real service, the user's
  config or the global menu. `--grab` requires `--fake`, selects the
  `offscreen` platform and writes one PNG; no window is ever mapped.

## Structure

| Part | Files | Responsibility |
| --- | --- | --- |
| API seam | `core/link_api.h`, `dbus_link_api.*`, `fake_link_api.*` | The only code that knows D-Bus names; the fake serves tests and `--fake` |
| Models | `core/json_list_model.*`, `message_json.*` | One list model over maps; pure row builders and the message merge |
| View models | `ui/*_view_model.*`, `launcher.h` | Devices, Threads, Conversation, PhoneApps, Notifications, Reply |
| Process seam | `core/process_runner.*` | The only way the app starts other programs |
| Menu | `app_shell/link_actions.*` | The AppShell catalog (every action needs a shortcut) |
| QML | `qml/` | Pages and parts; singletons from `QindaQt.Link` |

The view models are a static library (`qindaqt_link_app_view`) so tests link
them without a QML engine.

## Verification

| Row | Covers |
| --- | --- |
| `qindaqt.link-app-conversation` | Paging merge without duplicates, prepend/append, send confirmation once per session, cancel, send status and unsent recipients, local-row hand-over, MMS image fetch once, thread list paging, search, unread |
| `qindaqt.link-app-devices` | Paired phone description, missing service, pairing request (listed and live), accept/reject, stale request, unpair, setup report and failures, status without changes |
| `qindaqt.link-app-phone-apps` | Inventory parsing, mirror argv, favourites and persistence, needs-USB state, mute toggling, Settings macros launch, RCS flag |
| `qindaqt.link-app-options` | Reply-mode argv with `--`, option validation, `--grab` needs `--fake`, AppShell catalog accepted |
| `qindaqt.link-setup` | `PhoneSetup` against a fake `adb`: every grant, failure reporting, unsafe serials refused, status reads only |

Offscreen renders (`--fake … --grab`) of every page in populated, empty and
error states are the visual evidence.

## Known gaps

- `Link1.PendingPairings` belongs to the caller-policy slice. Until it lands the
  Devices page shows only requests that arrive while it is open.
- The input page reports "unavailable" until the input feature lands; its
  `desktopInputBuilt` constant flips then.
