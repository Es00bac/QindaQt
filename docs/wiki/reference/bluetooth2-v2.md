# Bluetooth2 protocol 2

Bluetooth2 is the additive pairing/trust revision of QindaQt's Bluetooth
contract. It shares the `org.qindaqt.Bluetooth1` service owner but uses the
separate `/org/qindaqt/Bluetooth2` object and `org.qindaqt.Bluetooth2`
interface. The frozen [Bluetooth1 protocol](bluetooth1-v1.md) remains
registered at its original object/interface with its original signatures,
four capability bits, and schema version 1.

The implementation is normative together with this page: the Bluetooth2
introspection XML, service `Q_CLASSINFO`, registered codecs, and protocol tests
must agree byte-for-byte. BlueZ remains sole authority for pairing, trust,
keys, device records, profiles, and authorization under
[ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md).

## Identity and fixed structures

- Bus name: `org.qindaqt.Bluetooth1` (session bus)
- Object path: `/org/qindaqt/Bluetooth2`
- Interface: `org.qindaqt.Bluetooth2`
- Schema version: `2`

| Value | D-Bus signature | Fields |
| --- | --- | --- |
| `Handle` | `(tt)` | `epoch`, `serial` |
| `Adapter` | `((tt)ssbb)` | `handle`, `address`, `name`, `powered`, `discovering` |
| `Device` | `((tt)(tt)ssuubbbnbyb)` | Bluetooth1 device fields followed by `trusted` |
| `PairingPrompt` | `(tu(tt)ssq)` | `promptId`, `kind`, device handle, bounded `detail`, bounded `serviceUuid`, entered digit count |
| `Snapshot` | `(uttuussa((tt)ssbb)a((tt)(tt)ssuubbbnbyb)(tu(tt)ssq))` | schema/lineage/state, adapters, devices, and one prompt |
| `OperationResult` | `(uuttttss)` | kind/status, initiating and observed lineage, reason, diagnostic |

Arrays and text retain Bluetooth1's bounds. Pairing text/service UUID is at
most 64 UTF-8 bytes; a PIN is 1–16 ASCII alphanumeric characters; Agent1
prompts expire after 60 seconds.

## Added enumerations and methods

Bluetooth2 retains capability bits 0–3 and adds `Pair=1<<4`,
`RemoveDevice=1<<5`, `SetTrusted=1<<6`, and `PairingPrompt=1<<7`.
Operation kinds retain values 0–4 and add `Pair=5`, `CancelPairing=6`,
`RemoveDevice=7`, `SetTrusted=8`, `ReplyConfirmation=9`, `ReplyPasskey=10`,
`ReplyPin=11`, and `CancelPrompt=12`. Prompt kinds are `None=0`,
`ConfirmPasskey=1`, `EnterPasskey=2`, `EnterPin=3`, `DisplayPasskey=4`,
`DisplayPin=5`, and `AuthorizeService=6`.

```text
GetSnapshot() -> snapshot
SetPowered(adapter (tt), powered b) -> result
AcquireDiscovery(adapter (tt)) -> result
ReleaseDiscovery(adapter (tt)) -> result
Connect(device (tt)) -> result
Disconnect(device (tt)) -> result
Pair(device (tt)) -> result
CancelPairing(device (tt)) -> result
Remove(device (tt)) -> result
SetTrusted(device (tt), trusted b) -> result
ReplyConfirmation(promptId t, accept b) -> result
ReplyPasskey(promptId t, passkey u) -> result
ReplyPin(promptId t, pin s) -> result
CancelPrompt(promptId t) -> result
Changed(epoch t, revision t)          (signal)
```

Ordinary mutations and prompt replies use separate serialized client lanes so
BlueZ may hold `Device1.Pair` while Agent1 awaits input. Every active prompt
has a nonzero ID unique for the backend lifetime. Every reply carries the ID
captured from the exact displayed prompt. The service, model, and BlueZ
adapter each reject a missing or mismatched ID; a delayed reply to prompt A
can never answer later prompt B, even for the same device and kind.

## Prompt and lifecycle rules

At most one prompt is active. Confirmation/authorization use
`ReplyConfirmation`; entry prompts accept only their matching reply method;
display prompts offer cancellation only. The inactive prompt is entirely
zero/empty, including `promptId`. An active prompt requires a nonzero ID and a
current device handle. Kind-specific unused fields must be empty; displayed
passkeys are exactly six digits; entered count is at most six.

The direct QtDBus adapter registers `KeyboardDisplay` Agent1 only while both
the exact `org.bluez` owner and at least one adapter exist. It calls
`UnregisterAgent` for the exact agent path before local-object teardown on
backend shutdown and when the last adapter disappears. Owner loss clears the
prompt and fences late calls even when the vanished daemon can no longer
receive unregister.

## Validation, uncertainty, and reasons

Bluetooth1's handle, inventory, text, collection, result-lineage, owner,
timeout, and no-replay validation rules remain. Bluetooth2 additionally
rejects schema versions other than 2, unknown bits above bit 7, invalid trusted
or prompt structure, a zero active prompt ID, nonzero inactive prompt data,
and prompt operations without a nonzero ID.

Stable pairing reason tokens include `paired`, `pairing-cancelled`,
`device-removed`, `trusted-set`, `prompt-replied`, `prompt-cancelled`,
`no-prompt`, `stale-prompt`, `wrong-prompt-kind`, `pairing-rejected`, and the
shared Bluetooth1 uncertainty/transport family. Cancellation always uses the
double-l `-cancelled` spelling.
