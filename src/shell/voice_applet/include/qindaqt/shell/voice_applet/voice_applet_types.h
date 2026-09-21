// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::VoiceApplet {

// What the consumer stack can currently promise the panel, independent of what
// the provider's own capture state is.
enum class ServicePhase {
    Unavailable,
    Starting,
    Ready,
};

// One offered control. An action that is present but not enabled is shown
// dimmed with its reason; an action the provider cannot perform at all is
// absent from the list entirely.
struct ActionModel {
    QString id;
    QString label;
    QString iconName;
    bool enabled = false;

    friend bool operator==(const ActionModel &, const ActionModel &) = default;
};

// The complete, bounded value model the QML chip and popup render.
//
// AGENT-CONTRACT: QML reads nothing but this struct. Every string here has
// already been through Voice1 validation, so QML never needs to sanitise, and
// must never reach past the controller for a "richer" value.
struct VoiceAppletModel {
    ServicePhase phase = ServicePhase::Unavailable;
    quint64 revision = 0;
    bool readGranted = false;
    bool controlGranted = false;
    bool enabled = false;
    bool capturing = false;
    bool commandMode = false;
    quint32 levelPercent = 0;

    // Stable, translatable-free identifiers for QML state selection.
    QString stateId;
    QString iconName;

    // Human-facing text, already composed so QML never concatenates sentences.
    QString summaryLabel;
    QString statusLabel;
    QString diagnostic;
    QString providerLabel;
    QString languageLabel;
    QString microphoneLabel;
    QString dictationShortcut;
    QString commandShortcut;
    QString routeLabel;
    QString accessibleName;
    QString accessibleDescription;

    // The live partial while capturing, otherwise the last delivered text.
    QString transcriptText;
    bool transcriptIsPartial = false;

    QList<ActionModel> actions;

    friend bool operator==(const VoiceAppletModel &, const VoiceAppletModel &) = default;
};

} // namespace QindaQt::Shell::VoiceApplet
