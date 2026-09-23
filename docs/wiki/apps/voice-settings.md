# Settings Voice route

The **Voice** route is where the desktop's own voice preferences live, and
where the attached speech provider's state, capabilities and identity are
shown. It consumes one Settings1 client and one
[`org.qindaqt.Voice1`](../architecture/voice-input.md) client, composed by the
route's own QML singleton.

## What the page owns

Two preferences, both in the `services` domain:

| Control | Key | Default |
| --- | --- | --- |
| **Voice input** | `services.voiceInput` | Off |
| **Show what you are saying in the panel** | `services.voicePanelTranscript` | On |

Off by default is deliberate: dictation records a microphone and, with a cloud
provider, sends that audio off the machine. The page opens with a plain
statement of exactly that, naming the difference between a cloud and a local
provider, and saying that QindaQt itself stores neither audio nor transcripts.

The route stays open and can save either preference while Voice1 is
disconnected. Opening it with Off or without a confirmed Settings1
baseline does not activate an installed provider. The provider card
distinguishes desktop opt-in from provider availability and the provider's
own shortcut-armed state. Applying Off withdraws this route's actions
at once; the shell and console follow the confirmed Settings1 change.

The second preference exists because the panel chip shows the words as they are
recognised. That is useful at a desk and wrong in a meeting room, so it is a
switch rather than a decision made for the user.

## What the provider owns

Everything else on the page is read from the provider and, where it can be
changed, changed through the contract:

- **Connection state** — provider connected or not, with a Retry control.
- **Speech provider** — a combo populated from the provider's own inventory.
  An entry marked unavailable is installed but not configured yet; it is shown
  rather than hidden, because hiding it hides a capability the user may have
  paid for and forgotten to finish setting up. The combo reflects the provider,
  not the click: it re-binds after every activation, so a refused switch snaps
  back instead of claiming a provider that is not in use.
- **What this provider can do** — the capability bits, each Yes or No.
- **Shortcuts, microphone, language, last insertion route.**
- **Dictation shortcuts armed** — arms or disarms push-to-talk without
  stopping the provider.
- **Try it** — Start dictating and Cancel. The page says plainly that dictation
  inserts into whatever window has keyboard focus, including the Settings
  window itself.

## Two keys, one commit at a time

Settings1 has no multi-key transaction, so the two preferences are drafted
together and committed one key at a time. A commit that fails partway leaves
the successful key applied and reports the failure; it never silently rolls the
other key back, because the user's own next action is the only thing that can
decide what they meant.

The second write waits for the fresh Settings1 snapshot and revision after
the first write; a confirmed commit reply alone is not a new baseline.
A clean draft follows external changes, while a dirty draft keeps its
requested values for review. An uncertain commit is never replayed.
The next snapshot tells the user which value actually landed. An explicit
Apply of Off also keeps this route's Voice1 use withdrawn through a conflict
or lost reply; only a fresh current-owner readback can reopen it when the
confirmed value is On.

## Actions deliberately absent

Finish, Retry, Undo and Copy are not offered here. They act on the last
dictation, which is in view in the [panel applet](../shell/voice-applet.md) and
the [Voice console](voice.md) and is not in view in Settings.

Vocabulary, corrections, command patterns and app profiles are not offered here
either: they belong to the speech provider and are reached through the
provider's own interface.

## Without a provider

The page renders fully. With Voice input Off it says that the desktop has
not connected; after a confirmed On with no provider it suggests
installing one. The provider's availability is separate from the
desktop preference; the preferences still
save, because they are the desktop's, not the provider's.

## See also

- [Voice input](../architecture/voice-input.md)
- [Voice applet](../shell/voice-applet.md)
- [Voice console](voice.md)
