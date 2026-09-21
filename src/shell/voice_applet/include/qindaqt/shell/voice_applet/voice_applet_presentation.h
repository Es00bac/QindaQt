// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>
#include <qindaqt/shell/voice_applet/voice_applet_types.h>

namespace QindaQt::Shell::VoiceApplet {

// Projects one bounded value model for the panel.
//
// `showTranscript` is the user's `services.voicePanelTranscript` preference.
// With it off, no partial and no delivered text is projected at all: the panel
// still reports that dictation is happening, and stops reporting what was
// said. That is the whole point of the switch, so it is applied here rather
// than in QML, where a second surface could forget it.
//
// AGENT-CONTRACT: this function is pure and is the single place that decides
// what the user sees and what they may touch. An action this function enables
// must be dispatchable; one it disables must be inert. It never consults the
// clock, the filesystem, or a session bus, so the whole projection is testable
// from a snapshot literal.
[[nodiscard]] VoiceAppletModel projectVoiceApplet(Services::Voice::ClientState clientState,
                                                  const QString &clientReasonCode,
                                                  bool hasSnapshot,
                                                  const Services::Voice::Snapshot &snapshot,
                                                  quint32 levelPercent,
                                                  bool readGranted,
                                                  bool controlGranted,
                                                  bool showTranscript);

// The action id a request kind is offered under, and the reverse. Empty for an
// id QML invented.
[[nodiscard]] QString actionIdForKind(Services::Voice::OperationKind kind);
[[nodiscard]] bool kindForActionId(const QString &actionId,
                                   Services::Voice::OperationKind &kind);

} // namespace QindaQt::Shell::VoiceApplet
