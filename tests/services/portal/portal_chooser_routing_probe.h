// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "portal_frontend_test_support.h"
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
namespace QindaQt::Tests::Portal {
class ChooserRoutingResponse final : public QObject {
    Q_OBJECT
public:
    int count = 0; quint32 response = 99; QString path; QVariantMap results;
public Q_SLOTS:
    void receive(quint32 value, const QVariantMap &values, const QDBusMessage &message) {
        ++count; response = value; results = values; path = message.path();
    }
};
// Real frontend -> native backend, with deliberately absent selected session.
// An actual failed Response proves routing while competing fakes stay unused.
inline bool nativeChooserRefusesWithoutAttachment(QDBusConnection bus, const QString &token, QString *error) {
    ChooserRoutingResponse receiver;
    if (!bus.connect(QString::fromLatin1(FrontendService), {}, QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"), &receiver, SLOT(receive(quint32,QVariantMap,QDBusMessage)))) {
        *error = QStringLiteral("cannot observe native chooser request"); return false;
    }
    auto call = QDBusMessage::createMethodCall(QString::fromLatin1(FrontendService), QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.FileChooser"), QStringLiteral("OpenFile"));
    call << QString{} << QStringLiteral("Native closed attachment proof") << QVariantMap{{QStringLiteral("handle_token"), token}};
    auto pending = bus.asyncCall(call, 5000);
    if (!waitUntil([&] { return pending.isFinished() && receiver.count == 1; }, 5000)) {
        *error = QStringLiteral("native FileChooser did not retire its frontend request"); return false;
    }
    const QDBusPendingReply<QDBusObjectPath> reply = pending;
    if (reply.isError() || reply.value().path() != receiver.path || receiver.response != 2 || !receiver.results.isEmpty()) {
        *error = QStringLiteral("native FileChooser did not fail closed without selected attachment"); return false;
    }
    return true;
}
} // namespace QindaQt::Tests::Portal
