# Audio VBAN streams

[ADR-0246](../adr/0246-configure-manual-audio-peers-through-audio1.md)
makes manual peer setup available in **Settings → Audio → Other computers**.
On the receiving computer, save the sender's exact IPv4 address, a stream
name, UDP port, and the physical speakers to play through; then enable
Receive. On the sending computer, save the same name and port, the receiving
computer's IPv4 address or host name, and the mixer bus; then enable Send.
For two-way audio, repeat with a second distinct stream name in the opposite
direction. Saving or editing a definition leaves it disabled until Enable is
chosen. Match each side's address to the peer's actual source/interface.

VBAN is open LAN UDP. It has no encryption, peer authentication, or discovery;
only enable receiving on a trusted network. The exact source IPv4 check blocks
packets arriving from a different address but is not cryptographic identity.
The Settings `Local route active` state confirms this computer's PipeWire
capture or receive-to-speaker links have reached a connected state. It does
not prove network packet delivery or that the other computer is audible.

The service stores definitions in
`$XDG_CONFIG_HOME/qindaqt/audio-vban.json` (or
`QINDAQT_AUDIO_VBAN_PATH`). Audio1 owns typed editing and atomic replacement;
manual editing is no longer needed. The outgoing shape is compatible with
ADR-0185. Incoming definitions now require `sourceHost` and
`outputNodeName`; older incoming entries without those fields are ignored
and cannot become wildcard listeners.

```json
{
  "outgoing": [
    { "name": "DeskToLaptop", "bus": "bus.a2", "host": "192.168.1.20", "port": 6980 }
  ],
  "incoming": [
    { "name": "LaptopToDesk", "sourceHost": "192.168.1.20",
      "outputNodeName": "alsa_output.usb-Speakers.analog-stereo", "port": 6981 }
  ]
}
```

| Field | Meaning |
|---|---|
| `name` | Unique VBAN stream name, up to 16 printable ASCII characters |
| `bus` (outgoing) | Local mixer bus to capture |
| `host` (outgoing) | Destination host name or IPv4 address |
| `sourceHost` (incoming) | Exact canonical IPv4 address permitted to send |
| `outputNodeName` (incoming) | Exact physical output node to play through; no fallback |
| `port` | UDP port 1–65535, default 6980; incoming ports must be unique |

An enabled sender waits for its bus target. An enabled receiver waits for its
selected output; if that output disappears, it never falls back to the
default output. The receive source also remains visible as a virtual input
for applications or console strips. Audio is 48 kHz stereo 16-bit PCM in
256-frame VBAN packets; other sample rates are ignored and incoming channel
counts are folded to stereo. See the [Audio1 schema-12 contract](audio1-v12.md).
