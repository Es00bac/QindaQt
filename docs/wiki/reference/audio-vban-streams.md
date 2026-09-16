# Audio VBAN streams

VBAN streams ([ADR-0185](../adr/0185-vban-is-a-document-and-two-threads.md))
are defined in `~/.config/qindaqt/audio-vban.json` (or the file named by
`QINDAQT_AUDIO_VBAN_PATH`). The service re-reads the file on every console
publication; Settings shows one switch per defined stream.

```json
{
  "outgoing": [
    { "name": "Desk", "bus": "bus.a2", "host": "192.168.1.20", "port": 6980 }
  ],
  "incoming": [
    { "name": "Laptop", "port": 6980 }
  ]
}
```

| Field | Meaning |
|---|---|
| `name` | The VBAN stream name, up to 16 ASCII characters; unique across both lists |
| `bus` (outgoing) | The bus to send: `bus.a1` … `bus.a5`, `bus.b1` … `bus.b3` |
| `host` (outgoing) | Host name or address to send to |
| `port` | UDP port; 6980 is the protocol's default |

An enabled outgoing stream runs only while its bus has a device. An enabled
incoming stream appears as an input device named `VBAN <name>`, which the
console's strips and any application can read.

The format on the wire is 48 kHz stereo 16-bit PCM in 256-frame packets, the
reference console's default; received streams at another sample rate are
ignored, and any channel count is folded to stereo.
