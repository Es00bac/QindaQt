// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/link_mirror/mirror_messages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace qindaqt::link_mirror {
namespace {

QByteArray controlFrame(std::uint8_t type, const QJsonObject &body)
{
    return encodeFrame(Channel::Control, type, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

std::optional<QJsonObject> object(const QByteArray &payload)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

bool isInt(const QJsonValue &value)
{
    return value.isDouble() && value.toDouble() == static_cast<double>(value.toInteger());
}

QString touchActionName(TouchAction action)
{
    switch (action) {
    case TouchAction::Down:
        return QStringLiteral("down");
    case TouchAction::Move:
        return QStringLiteral("move");
    case TouchAction::Up:
        return QStringLiteral("up");
    case TouchAction::Cancel:
        return QStringLiteral("cancel");
    }
    return QStringLiteral("cancel");
}

} // namespace

QByteArray helloFrame(const QString &token)
{
    return controlFrame(control::Hello, {{QStringLiteral("version"), kProtocolVersion},
                                         {QStringLiteral("token"), token},
                                         {QStringLiteral("client"), QStringLiteral("qindaqt-link-mirror")}});
}

QByteArray startFrame(const StartRequest &request)
{
    return controlFrame(control::Start, {{QStringLiteral("package"), request.packageName},
                                         {QStringLiteral("width"), request.size.width()},
                                         {QStringLiteral("height"), request.size.height()},
                                         {QStringLiteral("dpi"), request.dpi},
                                         {QStringLiteral("codec"), request.codec},
                                         {QStringLiteral("bitrateBps"), request.bitrateBps},
                                         {QStringLiteral("maxFps"), request.maxFps}});
}

QByteArray resizeFrame(QSize size, int dpi)
{
    return controlFrame(control::Resize, {{QStringLiteral("width"), size.width()},
                                          {QStringLiteral("height"), size.height()},
                                          {QStringLiteral("dpi"), dpi}});
}

QByteArray touchFrame(TouchAction action, int pointerId, QPointF position, double pressure)
{
    return controlFrame(control::Touch, {{QStringLiteral("action"), touchActionName(action)},
                                         {QStringLiteral("pointerId"), pointerId},
                                         {QStringLiteral("x"), position.x()},
                                         {QStringLiteral("y"), position.y()},
                                         {QStringLiteral("pressure"), pressure}});
}

QByteArray scrollFrame(QPointF position, double horizontal, double vertical)
{
    return controlFrame(control::Scroll, {{QStringLiteral("x"), position.x()},
                                          {QStringLiteral("y"), position.y()},
                                          {QStringLiteral("h"), horizontal},
                                          {QStringLiteral("v"), vertical}});
}

QByteArray keyFrame(bool down, int keycode, int metaState, int repeat)
{
    return controlFrame(control::Key, {{QStringLiteral("action"), down ? QStringLiteral("down") : QStringLiteral("up")},
                                       {QStringLiteral("keycode"), keycode},
                                       {QStringLiteral("meta"), metaState},
                                       {QStringLiteral("repeat"), repeat}});
}

QByteArray textFrame(const QString &text)
{
    return controlFrame(control::Text, {{QStringLiteral("text"), text}});
}

QByteArray syncFrame()
{
    return controlFrame(control::Sync, {});
}

QByteArray stopFrame()
{
    return controlFrame(control::Stop, {});
}

QByteArray pingFrame()
{
    return controlFrame(control::Ping, {});
}

QByteArray inventoryListFrame(const QStringList &packages, int iconSize)
{
    QJsonObject body{{QStringLiteral("iconSize"), iconSize}};
    if (!packages.isEmpty()) {
        body.insert(QStringLiteral("packages"), QJsonArray::fromStringList(packages));
    }
    return encodeFrame(Channel::Inventory, inventory::List, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

std::optional<ReadyInfo> parseReady(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body || !isInt(body->value(QStringLiteral("version")))) {
        return std::nullopt;
    }
    ReadyInfo info;
    info.version = body->value(QStringLiteral("version")).toInt();
    info.sdk = body->value(QStringLiteral("sdk")).toInt();
    info.release = body->value(QStringLiteral("release")).toString();
    info.model = body->value(QStringLiteral("model")).toString();
    for (const QJsonValue &entry : body->value(QStringLiteral("missing")).toArray()) {
        info.missing.append(entry.toString());
    }
    return info;
}

std::optional<DisplayInfo> parseDisplay(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body) {
        return std::nullopt;
    }
    const QJsonValue id = body->value(QStringLiteral("displayId"));
    const QJsonValue width = body->value(QStringLiteral("width"));
    const QJsonValue height = body->value(QStringLiteral("height"));
    if (!isInt(id) || !isInt(width) || !isInt(height)
        || !validStreamSize(width.toInteger(), height.toInteger())) {
        return std::nullopt;
    }
    return DisplayInfo{id.toInt(), QSize(width.toInt(), height.toInt()),
                       body->value(QStringLiteral("dpi")).toInt(), body->value(QStringLiteral("flags")).toInt()};
}

std::optional<ServerError> parseError(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body || !body->value(QStringLiteral("code")).isString()) {
        return std::nullopt;
    }
    return ServerError{body->value(QStringLiteral("code")).toString(),
                       body->value(QStringLiteral("message")).toString(),
                       body->value(QStringLiteral("fatal")).toBool(true)};
}

std::optional<VideoFormat> parseVideoFormat(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body) {
        return std::nullopt;
    }
    const QString codec = body->value(QStringLiteral("codec")).toString();
    const QJsonValue width = body->value(QStringLiteral("width"));
    const QJsonValue height = body->value(QStringLiteral("height"));
    if ((codec != QLatin1String("h264") && codec != QLatin1String("hevc")) || !isInt(width) || !isInt(height)
        || !validStreamSize(width.toInteger(), height.toInteger())) {
        return std::nullopt;
    }
    return VideoFormat{codec, QSize(width.toInt(), height.toInt())};
}

std::optional<AppEntry> parseApp(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body || !body->value(QStringLiteral("package")).isString()) {
        return std::nullopt;
    }
    AppEntry app;
    app.packageName = body->value(QStringLiteral("package")).toString();
    app.activity = body->value(QStringLiteral("activity")).toString();
    app.label = body->value(QStringLiteral("label")).toString(app.packageName);
    const QByteArray png = QByteArray::fromBase64(body->value(QStringLiteral("png")).toString().toLatin1());
    if (!png.isEmpty()) {
        app.icon.loadFromData(png, "PNG");
    }
    return app;
}

std::optional<int> parseInventoryEnd(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body || !isInt(body->value(QStringLiteral("count")))) {
        return std::nullopt;
    }
    return body->value(QStringLiteral("count")).toInt();
}

std::optional<bool> parseScreenState(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body) {
        return std::nullopt;
    }
    const QString state = body->value(QStringLiteral("state")).toString();
    if (state == QLatin1String("on")) {
        return true;
    }
    if (state == QLatin1String("off")) {
        return false;
    }
    return std::nullopt;
}

std::optional<QString> parseAppExited(const QByteArray &payload)
{
    const auto body = object(payload);
    if (!body) {
        return std::nullopt;
    }
    return body->value(QStringLiteral("reason")).toString(QStringLiteral("unknown"));
}

} // namespace qindaqt::link_mirror
