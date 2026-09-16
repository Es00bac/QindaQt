// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/macro_store.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>

#include <cmath>
#include <utility>

namespace QindaQt::Audio
{
namespace {

constexpr qint64 kMaxDocumentBytes = 256 * 1024;

[[nodiscard]] bool finiteNumber(const QJsonValue &value, double *out)
{
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return std::isfinite(*out);
}

} // namespace

MacroStore::MacroStore(QString path)
    : m_path(std::move(path))
{
}

QString MacroStore::defaultPath()
{
    const QByteArray override = qgetenv("QINDAQT_AUDIO_MACRO_PATH");
    if (!override.isEmpty()) {
        return QString::fromUtf8(override);
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("qindaqt/audio-macros.json"));
}

std::optional<OperationRequest> MacroStore::parseAction(const QJsonObject &action)
{
    const QString op = action.value(QStringLiteral("op")).toString();
    const QString strip = action.value(QStringLiteral("strip")).toString();
    const QString bus = action.value(QStringLiteral("bus")).toString();
    OperationRequest request;
    double number = 0.0;
    const auto boundedId = [](const QString &id) {
        return !id.isEmpty() && isBoundedText(id, kMaxConsoleIdUtf8Bytes);
    };
    if (op == QStringLiteral("strip.mute") && boundedId(strip)) {
        request.kind = OperationKind::SetStripMute;
        request.consoleId = strip;
        request.muted = action.value(QStringLiteral("on")).toBool(true);
        return request;
    }
    if (op == QStringLiteral("strip.solo") && boundedId(strip)) {
        request.kind = OperationKind::SetStripSolo;
        request.consoleId = strip;
        request.enabled = action.value(QStringLiteral("on")).toBool(true);
        return request;
    }
    if (op == QStringLiteral("strip.mono") && boundedId(strip)) {
        request.kind = OperationKind::SetStripMono;
        request.consoleId = strip;
        request.enabled = action.value(QStringLiteral("on")).toBool(true);
        return request;
    }
    if (op == QStringLiteral("strip.gain") && boundedId(strip)
        && finiteNumber(action.value(QStringLiteral("gainDb")), &number)) {
        request.kind = OperationKind::SetStripGain;
        request.consoleId = strip;
        request.gainDb = number;
        return request;
    }
    if (op == QStringLiteral("strip.send") && boundedId(strip)
        && action.value(QStringLiteral("bus")).isDouble()) {
        request.kind = OperationKind::SetStripSend;
        request.consoleId = strip;
        const int index = action.value(QStringLiteral("bus")).toInt(-1);
        if (index < 0 || index >= kMaxBuses) {
            return std::nullopt;
        }
        request.busIndex = static_cast<quint32>(index);
        request.enabled = action.value(QStringLiteral("on")).toBool(true);
        double gain = 0.0;
        if (action.contains(QStringLiteral("gainDb"))
            && !finiteNumber(action.value(QStringLiteral("gainDb")), &gain)) {
            return std::nullopt;
        }
        request.gainDb = gain;
        return request;
    }
    if (op == QStringLiteral("bus.mute") && boundedId(bus)) {
        request.kind = OperationKind::SetBusMute;
        request.consoleId = bus;
        request.muted = action.value(QStringLiteral("on")).toBool(true);
        return request;
    }
    if (op == QStringLiteral("bus.mono") && boundedId(bus)) {
        request.kind = OperationKind::SetBusMono;
        request.consoleId = bus;
        request.enabled = action.value(QStringLiteral("on")).toBool(true);
        return request;
    }
    if (op == QStringLiteral("bus.gain") && boundedId(bus)
        && finiteNumber(action.value(QStringLiteral("gainDb")), &number)) {
        request.kind = OperationKind::SetBusGain;
        request.consoleId = bus;
        request.gainDb = number;
        return request;
    }
    if (op == QStringLiteral("preset.load")) {
        const QString name = action.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || !isBoundedText(name, kMaxPresetNameUtf8Bytes)) {
            return std::nullopt;
        }
        request.kind = OperationKind::LoadPreset;
        request.displayName = name;
        return request;
    }
    return std::nullopt;
}

QList<Macro> MacroStore::load() const
{
    QList<Macro> macros;
    QFile file(m_path);
    if (!file.exists() || file.size() > kMaxDocumentBytes || !file.open(QIODevice::ReadOnly)) {
        return macros;
    }
    const QByteArray bytes = file.read(kMaxDocumentBytes + 1);
    if (bytes.size() > kMaxDocumentBytes) {
        return macros;
    }
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) {
        return macros;
    }
    for (const QJsonValue &entry : document.object().value(QStringLiteral("macros")).toArray()) {
        if (macros.size() >= kMaxMacros) {
            break;
        }
        const QJsonObject object = entry.toObject();
        Macro macro;
        macro.name = object.value(QStringLiteral("name")).toString().trimmed();
        if (macro.name.isEmpty() || !isBoundedText(macro.name, kMaxMacroNameUtf8Bytes)) {
            continue;
        }
        bool usable = true;
        for (const QJsonValue &actionValue : object.value(QStringLiteral("actions")).toArray()) {
            if (macro.actions.size() >= kMaxMacroActions) {
                usable = false;
                break;
            }
            const auto request = parseAction(actionValue.toObject());
            if (!request.has_value()) {
                usable = false;
                break;
            }
            macro.actions.append(MacroAction{.request = *request});
        }
        // AGENT-GUARD: a macro with one unusable action is dropped whole. A
        // button that ran the first half of what its author wrote is worse
        // than a button that is not there.
        bool duplicate = false;
        for (const Macro &existing : macros) {
            duplicate = duplicate || existing.name == macro.name;
        }
        if (usable && !macro.actions.isEmpty() && !duplicate) {
            macros.append(macro);
        }
    }
    return macros;
}

QStringList MacroStore::names(const QList<Macro> &macros)
{
    QStringList names;
    for (const Macro &macro : macros) {
        names.append(macro.name);
    }
    return names;
}

} // namespace QindaQt::Audio
