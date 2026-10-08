# Independent Audio console scale trace — source only

At exact5b7, WirePlumberWorker::finishApiLoading sets mixer scale1. applySendVolumes passes linearFromGainDb(edge.gainDb) times pan directly into per-channel set-volume. The actual gain helper computes pow(10, clamped/20), with exact bottom-stop zero.

Independently read [official WirePlumber0.5.18 mixer source](https://raw.githubusercontent.com/PipeWire/wireplumber/0.5.18/modules/module-mixer-api.c): scale1 is cubic, positive inputs are cubed, and per-channel volume uses that conversion. Therefore the current setter predicts -6dB becoming -18dB and a0.5 amplitude factor becoming0.125 before any downstream clipping. A console-only cube-root conversion is consistent with keeping ordinary percentage controls cubic. This is source-derived reasoning, not live audible proof or installed binary attestation. PipeWire max-volume10 was reported by Media but was not independently verified here.

Additional source risk for the owning audit: routingModuleArguments accepts linear but marks it unused and emits no initial gain/mute. applySendVolumes ignores the set-volume boolean. The author should examine the interval before a new node's mixer application succeeds; no transient sound or live cause is claimed.

No Audio source edit, native build, graph action, recording, device action or host bus use occurred. Media and root received the bounded corroboration. Bluetooth repaired8c02e21916a7cd9c84a8637dee274a80f66b50be remains immutable for Platform's exact recheck; no resources held.
