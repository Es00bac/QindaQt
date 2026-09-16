# Audio macro buttons

Macro buttons ([ADR-0183](../adr/0183-a-macro-button-is-a-list-of-console-operations.md))
are defined in `~/.config/qindaqt/audio-macros.json` (or the file named by
`QINDAQT_AUDIO_MACRO_PATH`). The service re-reads the file on every console
publication; an edit takes effect immediately.

```json
{
  "macros": [
    {
      "name": "Mute mic",
      "actions": [
        { "op": "strip.mute", "strip": "strip.hw.1", "on": true }
      ]
    },
    {
      "name": "Stream",
      "actions": [
        { "op": "preset.load", "name": "Stream night" },
        { "op": "strip.send", "strip": "strip.hw.1", "bus": 5, "on": true, "gainDb": -3 },
        { "op": "bus.mute", "bus": "bus.a2", "on": false }
      ]
    }
  ]
}
```

## Actions

| `op` | Fields | Operation |
|---|---|---|
| `strip.mute` | `strip`, `on` (default true) | mute or unmute a strip |
| `strip.solo` | `strip`, `on` | solo a strip |
| `strip.mono` | `strip`, `on` | fold a strip to mono |
| `strip.gain` | `strip`, `gainDb` | set a strip's fader, −60 … +12 dB |
| `strip.send` | `strip`, `bus` (index 0–7), `on`, `gainDb` | switch a matrix cell and set its gain |
| `bus.mute` | `bus`, `on` | mute or unmute a bus |
| `bus.mono` | `bus`, `on` | fold a bus to mono |
| `bus.gain` | `bus`, `gainDb` | set a bus fader |
| `preset.load` | `name` | recall a preset by its display name |

Strip ids are `strip.hw.1` … `strip.hw.5` and `strip.virtual.1` …
`strip.virtual.3`; bus ids are `bus.a1` … `bus.a5` and `bus.b1` … `bus.b3`.
Bus indexes in `strip.send` are 0–4 for A1–A5 and 5–7 for B1–B3.

## Rules

- A macro whose actions include anything not listed above is not offered at
  all; a button never runs half of its script.
- At most 32 macros of at most 16 actions; names are at most 64 bytes and
  must be unique (the first wins).
- Actions run in order as ordinary console operations. The first one the
  service refuses stops the macro; the Settings status line shows why, and
  the actions before it stay applied.
