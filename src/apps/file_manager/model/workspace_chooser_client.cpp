// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/workspace_chooser_client.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr auto serviceName = "org.qindaqt.Compositor1";
constexpr auto objectPath = "/org/qindaqt/Compositor";
constexpr auto method = "ChooseApplicationForActivePicker";
// A picker click must feel immediate; the compositor answers after one
// desktop-entry dispatch, never after the application's window arrives.
constexpr int replyTimeoutMilliseconds = 10000;

} // namespace

ChooserReply chooseApplicationOnCompositor(const QString &desktopEntryId)
{
    QDBusInterface compositor(serviceName, objectPath, QString(),
                              QDBusConnection::sessionBus());
    if (!compositor.isValid()) {
        return {false,
                QStringLiteral("The compositor's workspace service is unavailable.")};
    }
    const auto reply = compositor.callWithArgumentList(
        QDBus::BlockWithGui, method, {QVariant(desktopEntryId)});
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return {false, reply.errorMessage()};
    }
    if (reply.arguments().isEmpty()) {
        return {false, QStringLiteral("The compositor returned no answer.")};
    }
    const auto document = QJsonDocument::fromJson(
        reply.arguments().constFirst().value<QByteArray>());
    const auto object = document.object();
    const auto status = object.value(QStringLiteral("status")).toString();
    const auto message = object.value(QStringLiteral("message")).toString();
    if (status == QLatin1String("ok")) {
        return {true, message};
    }
    return {false, message.isEmpty()
                       ? QStringLiteral("The application choice was rejected.")
                       : message};
}

} // namespace QindaQt::Apps::FileManager
