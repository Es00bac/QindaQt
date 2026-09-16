# ADR-0176: the console remembers itself

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** None

## Context

[ADR-0173](0173-the-mixing-console-slice-of-audio1.md) gave the console a
document form — `ConsoleModel::toJson` and `loadJson`, carrying labels, gains,
mutes and routing and deliberately nothing live — and nothing ever called
either. Every restart of the audio service, and so every login, put the user's
faders back at unity and emptied their routing matrix. A console that forgets
itself is a demo.

## Decision

One document, `$XDG_CONFIG_HOME/qindaqt/audio-console.json`, beside the
settings service's own, owned by a `ConsoleStore` the resident service holds.

**Restore before the backend runs.** The store loads into the coordinator's
model in the resident service's constructor, before `start()`. The first graph
publication then binds the *user's* console, and the virtual endpoints are
declared with the user's labels, rather than a default console being published
and replaced a moment later.

**Save on every publication, debounced, deduplicated.** The resident service
starts a 500 ms single-shot timer on every `snapshotChanged`, and the store
skips a write whose bytes match the last document it wrote. A burst of fader
moves is one write; a graph change that alters nothing the document carries is
no write at all. A pending debounce is flushed on `stop()`, so a shutdown never
loses the last move.

**Atomic and fail-closed.** Writes go through `QSaveFile`: the document is
complete on disk or the previous one is still there. Loads refuse a file over
256 KiB before reading it, re-check the size after (a file can grow between the
two calls), and refuse anything that is not a JSON object; a document that
passes is applied through the model's own per-entry tolerance, so one corrupt
strip costs that strip its settings, not the user their console.

**Tests never touch the user's console.** The resident service takes the store
path as a constructor argument; a test host passes a temporary directory.

## Consequences

- Faders, mutes, solos, labels and the whole routing matrix survive a service
  restart, a logout and a reboot.
- Nothing live is persisted, so a restored console is unbound until the graph
  binds it — by name for the console's own endpoints ([ADR-0175](0175-virtual-strips-and-buses-are-nodes-the-console-owns.md)),
  by the automatic rule for hardware.
- The document is the natural carrier for presets: a preset is this document
  under another name.

## Revisit when

- Explicit per-strip device pins land; the pin is a `node.name` and belongs in
  this document, which is why devices now carry their names on the wire.
- Presets and a "save as" surface are built on top of the store.
