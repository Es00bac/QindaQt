// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_device_probe.h"
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>

namespace QindaQt::Apps::RemovableMedia {
namespace Public = QindaQt::RemovableMedia;
namespace {
QDBusMessage read(const QDBusConnection &reader, QDBusMessage request)
{
    request.setAutoStartService(false);
    QDBusPendingCallWatcher pending(reader.asyncCall(request, 2000));
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&pending, &QDBusPendingCallWatcher::finished, &loop, &QEventLoop::quit);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(2500);
    if (!pending.isFinished()) loop.exec();
    return pending.isFinished() ? pending.reply()
        : QDBusMessage::createError(QStringLiteral("org.qindaqt.Probe.Timeout"), QString());
}
}
int verifyDevicesContract(const QDBusConnection &owner, const QString &mountedRoot)
{
    // AGENT-GUARD: address only this probe's unique owner. Even an accidental
    // run on the real user bus cannot activate or inspect the resident watcher.
    const QString connection = QStringLiteral("removable-media-device-probe-reader");
    const auto reader = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connection);
    auto versionRequest = QDBusMessage::createMethodCall(owner.baseService(),
        QString::fromLatin1(Public::kObjectPath), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    versionRequest.setArguments({QString::fromLatin1(Public::kInterfaceName), QStringLiteral("Version")});
    const QDBusReply<QDBusVariant> version = read(reader, versionRequest);
    const QDBusReply<QByteArray> reply = read(reader, QDBusMessage::createMethodCall(owner.baseService(),
        QString::fromLatin1(Public::kObjectPath), QString::fromLatin1(Public::kInterfaceName),
        QStringLiteral("GetSnapshot")));
    QDBusConnection::disconnectFromBus(connection);
    Public::Snapshot snapshot;
    if (!version.isValid() || version.value().variant().toUInt() != Public::kProtocolVersion
        || !reply.isValid() || !Public::decodeSnapshot(reply.value(), snapshot).succeeded()
        || snapshot.lineage.owner != owner.baseService()
        || snapshot.availability != Public::Availability::Ready || snapshot.rows.size() != 1
        || snapshot.rows[0].mountState != Public::MountState::Mounted
        || snapshot.rows[0].preferredRoot != mountedRoot
        || snapshot.rows[0].mountRoots != QStringList{mountedRoot}
        || !snapshot.rows[0].actions.open.enabled || !snapshot.rows[0].actions.remove.enabled) {
        std::fprintf(stderr, "qindaqt-removable-media: Devices export contract failed (synthetic media only)\n");
        return 5;
    }
    std::puts("removable-media-devices-ready fixture-only");
    return 0;
}
}
