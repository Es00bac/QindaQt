# Full Audio audit source findings checkpoint

Exact cohort: 5b7f5b5910e262a4bf2be2326bc92fc97af4e2ac. This is a source-traced checkpoint; the full capability audit remains in progress. No new test or host action is executed.

## Priority gain/Mono/trim gap

SetStripGain and SetBusGain mutate stored fields in console_model.cpp (247–252, 336–341), and the Settings/client/service path returns configuration success. ConsoleModel::routing (451–470) uses only MatrixSend.gainDb. publishRouting (audio_console_operations.cpp 269–304) forwards only that value. Worker applySendVolumes (wireplumber_worker_routing.cpp 97–145) writes only the declared send gain with pan, or zero when mute/solo silences it. An owning-source-wide search finds no other strip or bus fader gain consumer.

Thus changing a strip or bus fader alone leaves every declared backend edge identical, while the UI echoes the changed field. The current faderMuteSoloMonoAndPanApply test checks stored fields; routingCarriesPerSendGain checks send gains without changed strip/bus gains. The ordinary SetVolume backend for devices and application streams is separate and implemented.

Strip/Bus Mono and Strip channelTrimDb similarly remain stored/public values without a backend routing/processing consumer. These remain subsequent gaps. Root will assign a separate exact gain-only repair after the full audit freeze. Its acceptance must prove declared edges and actual private audio amplitude, independent send gain, bottom-stop silence, pan, mute/solo and physical bus processing; an echoed field alone is insufficient.

## Default choice and multi-route semantics

SetDefault calls default-nodes-api set-default-configured-node-name (wireplumber_worker_operations.cpp 242–255). MoveStream independently writes one stream's target.object (361–379). It does not issue moves for all streams or rewrite saved console sends.

However automatic hardware bindings are recomputed every accepted snapshot: default first, then serial, excluding claimed pins (audio_console_operations.cpp 57–90, 121–193). A default change can change Automatic A1/A2 destinations. Explicit bus/strip pins resolve their persisted node.name and remain unbound when absent. No actual installed pins or stream state are inspected here. Mixed pinned/automatic/default-change coverage is required before interpreting a connected speaker as a full multi-route migration.

## Documentation and reference provenance

Current parity/docs contain stale claims: SetBusTarget is still listed planned despite ADR0178 and implemented pickers; physical bus delay is listed planned despite schema14; startup restore is listed planned despite ResidentAudioService restoring before backend start. Audio-service's future applet text contradicts its implemented owner. Audio applet default-picker and keyboard-step paragraphs disagree internally. Correct these through owning documentation, while retaining explicit actual missing features.

Only official VB-Audio primary references are used. The current manual URL serves SEP2026 version3.1.3.0, 12,180,012 bytes, SHA256 dddca1a3ddfe21ddc48908afde16e9e5fe54b17ae6b438fe5065adbbc98d8f10. Indexed search still labels an older Dec2025 manual; actual downloaded bytes control this audit's date/version attribution. No VB source or long manual passages are copied.
