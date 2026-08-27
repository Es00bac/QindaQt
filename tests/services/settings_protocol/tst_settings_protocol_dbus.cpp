// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::SettingsProtocol;

namespace {

class WireDecodeProbe final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.SettingsDecodeProbe")

public Q_SLOTS:
    QVariantMap Decode(QDBusVariant input)
    {
        QVariant value = input.variant();
        input = QDBusVariant{};
        BoundedSettingsValueCodec::Usage usage;
        QString error;
        const auto decoded = decodeBoundedJsonValue(value, &error, &usage);
        return {{QStringLiteral("opaque"),
                 value.metaType() == QMetaType::fromType<QDBusArgument>()},
                {QStringLiteral("ok"), decoded.has_value()},
                {QStringLiteral("nodes"), qlonglong(usage.nodes)},
                {QStringLiteral("bytes"), qlonglong(usage.bytes)},
                {QStringLiteral("error"), error}};
    }

};

QVariantMap completedMapCall(const QDBusConnection &connection,
                             const QString &method,
                             const QVariantList &arguments)
{
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.qindaqt.SettingsDecodeProbe"),
        QStringLiteral("/org/qindaqt/SettingsDecodeProbe"),
        QStringLiteral("org.qindaqt.SettingsDecodeProbe"), method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        finished.wait(5'000);
    }
    const QDBusPendingReply<QVariantMap> reply(watcher);
    return reply.isValid()
               ? reply.value()
               : QVariantMap{{QStringLiteral("callError"), reply.error().message()}};
}

} // namespace

class SettingsProtocolDbusTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rejectsOpaqueNodeByteAndEnvelopeOverflowWhileStreaming();
};

void SettingsProtocolDbusTests::rejectsOpaqueNodeByteAndEnvelopeOverflowWhileStreaming()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceName = QStringLiteral("settings-decode-probe-") + suffix;
    const QString clientName = QStringLiteral("settings-decode-client-") + suffix;
    auto serviceBus = QDBusConnection::connectToBus(address, serviceName);
    auto clientBus = QDBusConnection::connectToBus(address, clientName);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());

    WireDecodeProbe probe;
    QVERIFY(serviceBus.registerService(QStringLiteral("org.qindaqt.SettingsDecodeProbe")));
    QVERIFY(serviceBus.registerObject(QStringLiteral("/org/qindaqt/SettingsDecodeProbe"),
                                      &probe, QDBusConnection::ExportAllSlots));

    const QVariantMap legal{{QStringLiteral("outputs"),
                             QVariantList{QVariantMap{{QStringLiteral("name"),
                                                       QStringLiteral("Virtual-1")},
                                                      {QStringLiteral("position"),
                                                       QVariantList{0, 0}}}}}};
    const auto accepted = completedMapCall(
        clientBus, QStringLiteral("Decode"),
        {QVariant::fromValue(QDBusVariant(QVariant::fromValue(legal)))});
    QVERIFY(accepted.value(QStringLiteral("opaque")).toBool());
    QVERIFY(accepted.value(QStringLiteral("ok")).toBool());

    QVariantList nodeOverflow;
    for (qsizetype index = 0; index < WireContract::MaximumListEntries - 12; ++index) {
        nodeOverflow.append(QVariant::fromValue(QVariantList(8, true)));
    }
    QCOMPARE(nodeOverflow.size(), WireContract::MaximumListEntries - 12);
    const auto rejectedNodes = completedMapCall(
        clientBus, QStringLiteral("Decode"),
        {QVariant::fromValue(QDBusVariant(QVariant::fromValue(nodeOverflow)))});
    QVERIFY(rejectedNodes.value(QStringLiteral("opaque")).toBool());
    QVERIFY(!rejectedNodes.value(QStringLiteral("ok")).toBool());
    const QString nodeError = rejectedNodes.value(QStringLiteral("error")).toString();
    QVERIFY2(nodeError.contains(QStringLiteral("nodes")), qPrintable(nodeError));

    QVariantList byteOverflow;
    const QString chunk(16'000, QLatin1Char('x'));
    for (int index = 0; index < 20; ++index) {
        byteOverflow.append(chunk);
    }
    const auto rejectedBytes = completedMapCall(
        clientBus, QStringLiteral("Decode"),
        {QVariant::fromValue(QDBusVariant(QVariant::fromValue(byteOverflow)))});
    QVERIFY(rejectedBytes.value(QStringLiteral("opaque")).toBool());
    QVERIFY(!rejectedBytes.value(QStringLiteral("ok")).toBool());
    QVERIFY(rejectedBytes.value(QStringLiteral("error")).toString().contains(
        QStringLiteral("aggregate bytes")));

    serviceBus.unregisterObject(QStringLiteral("/org/qindaqt/SettingsDecodeProbe"));
    serviceBus.unregisterService(QStringLiteral("org.qindaqt.SettingsDecodeProbe"));
    QDBusConnection::disconnectFromBus(serviceName);
    QDBusConnection::disconnectFromBus(clientName);
    daemon.terminate();
    QVERIFY(daemon.waitForFinished());
}

QTEST_GUILESS_MAIN(SettingsProtocolDbusTests)
#include "tst_settings_protocol_dbus.moc"
