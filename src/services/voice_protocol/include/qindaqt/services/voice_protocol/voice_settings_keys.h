// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

namespace QindaQt::Services::Voice {

// AGENT-NOTE: these are QindaQt's own Settings1 keys, not part of the Voice1
// wire contract — a provider never sees them. They live in the protocol module
// because it is the one target both the shell applet and the Settings Voice
// route already depend on, and a key spelled twice is a key that drifts.

// Whether QindaQt starts and uses a speech provider at all. Off by default:
// dictation records a microphone, and with a cloud provider sends that audio
// off the machine.
inline constexpr char kVoiceInputSettingsKey[] = "services.voiceInput";

// Whether the panel shows the words as they are recognised. On by default;
// off is for a panel other people can see.
inline constexpr char kVoicePanelTranscriptSettingsKey[] =
    "services.voicePanelTranscript";

} // namespace QindaQt::Services::Voice
