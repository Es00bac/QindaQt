// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace QindaQt::Audio
{

// Macro buttons (ADR-0183): a user-written document of named action lists,
// read fail-closed. The console runs a macro as the console operations its
// actions describe; it never invents an operation the wire does not have.
//
// The document is `$XDG_CONFIG_HOME/qindaqt/audio-macros.json`:
//   { "macros": [ { "name": "Mute mic",
//                   "actions": [ { "op": "strip.mute", "strip": "strip.hw.1", "on": true } ] } ] }
// Actions: strip.mute, strip.solo, strip.mono, strip.gain (gainDb),
// strip.send (bus, on, gainDb), bus.mute, bus.mono, bus.gain (gainDb),
// preset.load (name).
struct MacroAction {
    OperationRequest request;

    friend bool operator==(const MacroAction &, const MacroAction &) = default;
};

struct Macro {
    QString name;
    QList<MacroAction> actions;

    friend bool operator==(const Macro &, const Macro &) = default;
};

class MacroStore final
{
public:
    explicit MacroStore(QString path);
    // `$XDG_CONFIG_HOME/qindaqt/audio-macros.json`, or QINDAQT_AUDIO_MACRO_PATH.
    [[nodiscard]] static QString defaultPath();

    // Re-reads the document. Returns the macros that parsed; an action that
    // does not parse drops its whole macro rather than running a truncated
    // one, and a document that is not an object yields none.
    [[nodiscard]] QList<Macro> load() const;
    [[nodiscard]] static QStringList names(const QList<Macro> &macros);
    // Parses one action; nullopt when it is not one of the documented forms.
    [[nodiscard]] static std::optional<OperationRequest> parseAction(const QJsonObject &action);

private:
    QString m_path;
};

} // namespace QindaQt::Audio
