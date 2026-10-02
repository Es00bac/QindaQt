// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/capture_types.h>
#include <qindaqt/services/portal/access_consent.h>
#include <QDBusArgument>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace QindaQt::Services::Portal {
namespace {
bool typed(const QVariantMap &map, const QString &key, int type) { return !map.contains(key) || map.value(key).metaType().id() == type; }
bool validText(const QString &value, qsizetype limit) {
    if (value.size() > limit) return false;
    for (const auto c : value) if (c.isNull() || c.category() == QChar::Other_Control) return false;
    return true;
}
bool integer(const QJsonValue &value, int low, int high) { const double d = value.toDouble(-1); return value.isDouble() && std::isfinite(d) && std::floor(d) == d && d >= low && d <= high; }
}
std::optional<CaptureRequest> screenshotRequest(const QString &app, const QString &parent, const QVariantMap &options, bool color) {
    // Reuse only the public parent syntax contract; question presentation is separate.
    if (!validText(app, 255) || !accessQuestion(app, parent, {}, {}, {}, {})) return {};
    for (const auto &key : options.keys()) if (key != "modal" && key != "interactive" && key != "permission_store_checked") return {};
    if (!typed(options, "modal", QMetaType::Bool) || !typed(options, "interactive", QMetaType::Bool)
        || !typed(options, "permission_store_checked", QMetaType::Bool)) return {};
    return CaptureRequest{color ? CaptureKind::Color : CaptureKind::Screenshot, app, parent, {}, {}, options.value("interactive", false).toBool(), options.value("modal", true).toBool()};
}
bool validScreenCastSelection(const QVariantMap &options) {
    for (const auto &key : options.keys())
        if (key != "types" && key != "multiple" && key != "cursor_mode" && key != "persist_mode" && key != "restore_data") return false;
    // AGENT-NOTE: the frontend forwards persist_mode and swaps restore_token for
    // its stored (suv) restore_data (xdg-desktop-portal 1.20 screen-cast.c).
    // Callers such as OBS always send persist_mode, so refusing it broke them.
    // Nothing is persisted: Start never returns restore_data.
    if (options.contains("restore_data")) {
        const auto value = options.value("restore_data");
        if (value.metaType() != QMetaType::fromType<QDBusArgument>()) return false;
        const auto argument = value.value<QDBusArgument>();
        if (argument.currentSignature() != QLatin1String("(suv)")) return false;
    }
    return typed(options, "types", QMetaType::UInt) && typed(options, "multiple", QMetaType::Bool)
        && typed(options, "cursor_mode", QMetaType::UInt) && typed(options, "persist_mode", QMetaType::UInt)
        && options.value("types", 1U).toUInt() == 1 && options.value("persist_mode", 0U).toUInt() <= 2
        && QSet<quint32>{1, 2, 4}.contains(options.value("cursor_mode", 1U).toUInt());
}
QString captureCaller(const QString &path) {
    static const QRegularExpression pattern("^/org/freedesktop/portal/desktop/request/([0-9]+_[0-9]+)/[A-Za-z0-9_]+$");
    const auto match = pattern.match(path);
    return path.size() <= 512 && match.hasMatch() ? ':' + match.captured(1).replace('_', '.') : QString{};
}
bool validCapturePublication(CaptureKind kind, const QVariantMap &results) {
    if (results.size() != 1) return false;
    if (kind == CaptureKind::Screenshot) {
        const auto value = results.value("uri"); const auto text = value.toString(); const QUrl uri(text, QUrl::StrictMode);
        return value.metaType().id() == QMetaType::QString && text.size() <= 8192 && uri.isValid() && uri.isLocalFile()
            && uri.host().isEmpty() && !uri.hasQuery() && !uri.hasFragment() && QDir::isAbsolutePath(uri.toLocalFile())
            && QDir::cleanPath(uri.toLocalFile()) == uri.toLocalFile() && uri.toString(QUrl::FullyEncoded) == text;
    }
    if (kind == CaptureKind::Color) {
        const auto value = results.value("color"); if (value.metaType() != QMetaType::fromType<CaptureColor>()) return false;
        const auto color = value.value<CaptureColor>();
        for (const double channel : {color.red, color.green, color.blue}) if (!std::isfinite(channel) || channel < 0 || channel > 1) return false;
        return true;
    }
    const auto value = results.value("streams"); if (value.metaType() != QMetaType::fromType<CaptureStreams>()) return false;
    const auto streams = value.value<CaptureStreams>(); if (streams.isEmpty() || streams.size() > 16) return false;
    QSet<quint32> nodes;
    for (const auto &stream : streams) {
        if (!stream.node || nodes.contains(stream.node) || stream.properties.size() != 2) return false;
        nodes.insert(stream.node);
        for (const auto &key : {QStringLiteral("position"), QStringLiteral("size")}) if (stream.properties.value(key).metaType() != QMetaType::fromType<CaptureCoordinate>()) return false;
        const auto size = stream.properties.value("size").value<CaptureCoordinate>();
        if (size.first <= 0 || size.first > 16384 || size.second <= 0 || size.second > 16384) return false;
    }
    return true;
}
QJsonObject captureFrame(const CaptureRequest &r, const QString &directory, const QString &owner) {
    QJsonObject frame{{"kind", static_cast<int>(r.kind)}, {"app", r.app}, {"parent", r.parent}, {"session", r.session},
        {"interactive", r.interactive}, {"modal", r.modal}, {"directory", directory}, {"owner", owner}};
    if (r.kind == CaptureKind::Stream) { frame.insert("multiple", r.multiple); frame.insert("cursor_mode", static_cast<int>(r.cursorMode)); }
    return frame;
}
std::optional<CaptureRequest> captureRequestFromFrame(const QJsonObject &frame) {
    if ((frame.size() != 8 && frame.size() != 10) || !integer(frame.value("kind"), 0, 2) || !frame.value("app").isString()
        || !frame.value("parent").isString() || !frame.value("session").isString()
        || !frame.value("interactive").isBool() || !frame.value("modal").isBool()
        || !frame.value("directory").isString() || !frame.value("owner").isString()) return {};
    const auto request = screenshotRequest(frame.value("app").toString(), frame.value("parent").toString(), {}, frame.value("kind").toInt() == 1);
    if (!request || !QDir::isAbsolutePath(frame.value("directory").toString()) || !frame.value("owner").toString().startsWith(':')) return {};
    auto result = *request; result.kind = static_cast<CaptureKind>(frame.value("kind").toInt());
    result.session = frame.value("session").toString(); result.interactive = frame.value("interactive").toBool(); result.modal = frame.value("modal").toBool();
    if (frame.size() == 10) {
        if (result.kind != CaptureKind::Stream || !frame.value("multiple").isBool() || !integer(frame.value("cursor_mode"), 1, 4)
            || !QSet<int>{1, 2, 4}.contains(frame.value("cursor_mode").toInt())) return {};
        result.multiple = frame.value("multiple").toBool(); result.cursorMode = static_cast<quint32>(frame.value("cursor_mode").toInt());
    }
    if (result.kind == CaptureKind::Stream && result.session.isEmpty()) return {};
    return result;
}
std::optional<QVariantMap> captureResults(CaptureKind kind, const QJsonObject &object, const QString &directory) {
    if (kind == CaptureKind::Screenshot) {
        if (object.size() != 1 || !object.value("uri").isString()) return {};
        const QString expected = QUrl::fromLocalFile(QDir(directory).filePath("screenshot.png")).toString(QUrl::FullyEncoded);
        if (object.value("uri").toString() != expected) return {};
        return QVariantMap{{"uri", expected}};
    }
    if (kind == CaptureKind::Color) {
        const auto array = object.value("color").toArray();
        if (object.size() != 1 || array.size() != 3) return {};
        for (const auto &v : array) if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() < 0 || v.toDouble() > 1) return {};
        return QVariantMap{{"color", QVariant::fromValue(CaptureColor{array[0].toDouble(), array[1].toDouble(), array[2].toDouble()})}};
    }
    // AGENT-CONTRACT: a batch is published atomically after every consented
    // source starts; any malformed member rejects the entire helper result.
    if (object.size() == 1 && object.value("streams").isArray()) {
        const auto batch = object.value("streams").toArray(); if (batch.isEmpty() || batch.size() > 16) return {};
        CaptureStreams streams;
        for (const auto &member : batch) {
            if (!member.isObject() || member.toObject().size() != 6 || !member.toObject().contains("node")) return {};
            const auto parsed = captureResults(kind, member.toObject(), directory);
            if (!parsed) return {};
            streams.append(parsed->value("streams").value<CaptureStreams>());
        }
        QVariantMap result{{"streams", QVariant::fromValue(streams)}};
        return validCapturePublication(kind, result) ? std::optional<QVariantMap>{result} : std::nullopt;
    }
    if (object.size() != 6 || !integer(object.value("node"), 1, 2147483647)
        || !integer(object.value("x"), -100000, 100000) || !integer(object.value("y"), -100000, 100000)
        || !integer(object.value("width"), 1, 16384) || !integer(object.value("height"), 1, 16384)
        || !object.value("name").isString() || !validText(object.value("name").toString(), 256)) return {};
    // Position/size use the exact (ii) wire below, not a QVariantList signature.
    QVariantMap properties{{"position", QVariant::fromValue(CaptureCoordinate{object.value("x").toInt(), object.value("y").toInt()})},
        {"size", QVariant::fromValue(CaptureCoordinate{object.value("width").toInt(), object.value("height").toInt()})}};
    return QVariantMap{{"streams", QVariant::fromValue(CaptureStreams{{static_cast<quint32>(object.value("node").toInt()), properties}})}};
}
}
