// SPDX-License-Identifier: GPL-3.0-or-later

#include "../audio_service/support/fake_audio_backend.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/qt_audio_transport.h>
#include <qindaqt/services/audio_protocol/audio_dbus.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_service/resident_audio_service.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

#include <memory>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

namespace
{

class PrivateBus final
{
public:
    bool start()
    {
        process.setProgram(QStringLiteral("dbus-daemon"));
        process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-audio-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus()
    {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

} // namespace

class QtAudioTransportTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void successiveOwnersAndDelayedOperation();
};

void QtAudioTransportTests::successiveOwnersAndDelayedOperation()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.AudioTest.p%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString firstConnectionName = bus.name + QStringLiteral("-service-1");
    QDBusConnection firstConnection =
        QDBusConnection::connectToBus(bus.address, firstConnectionName);
    QVERIFY(firstConnection.isConnected());

    auto firstBackend = std::make_unique<FakeAudioBackend>();
    FakeAudioBackend *firstBackendPtr = firstBackend.get();
    auto firstHost = std::make_unique<ResidentAudioService>(
        std::move(firstBackend), firstConnection, serviceName);
    QCOMPARE(firstHost->start(), ServiceStartStatus::Started);
    firstBackendPtr->publish(audioSnapshot(41, 2));

    const QDBusMessage introspectionCall = QDBusMessage::createMethodCall(
        serviceName, QString::fromLatin1(kObjectPath),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));
    QDBusPendingCallWatcher introspectionWatcher(
        bus.connection.asyncCall(introspectionCall));
    QSignalSpy introspectionFinished(&introspectionWatcher,
                                     &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE(introspectionFinished.size(), 1);
    const QDBusPendingReply<QString> introspectionReply = introspectionWatcher;
    QVERIFY2(!introspectionReply.isError(),
             qPrintable(introspectionReply.error().message()));
    const QString introspection = introspectionReply.value();
    QVERIFY(introspection.contains(
        QStringLiteral("type=\"(uttuuss(tt)(tt)a((tt)ussdbbbbbb)")));
    QVERIFY(introspection.contains(QStringLiteral("type=\"(uuttttss)\"")));

    QtAudioTransport transport(bus.connection, serviceName);
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    QTRY_COMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().epoch, quint64(41));
    QVERIFY(client.owner().startsWith(QLatin1Char(':')));

    const quint64 requestId = client.setMute({.epoch = 41, .serial = 10}, true);
    QTRY_COMPARE(firstBackendPtr->operations.size(), 1);
    firstBackendPtr->finish(firstBackendPtr->operations[0].operationId,
                            {.status = BackendOperationStatus::Succeeded,
                             .reasonCode = QStringLiteral("ok"),
                             .diagnostic = {}});
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Succeeded);

    const QString firstOwner = client.owner();
    firstHost->stop();
    firstHost.reset();
    QDBusConnection::disconnectFromBus(firstConnectionName);
    QTRY_COMPARE(client.state(), ClientState::Unavailable);

    const QString secondConnectionName = bus.name + QStringLiteral("-service-2");
    QDBusConnection secondConnection =
        QDBusConnection::connectToBus(bus.address, secondConnectionName);
    QVERIFY(secondConnection.isConnected());
    auto secondBackend = std::make_unique<FakeAudioBackend>();
    FakeAudioBackend *secondBackendPtr = secondBackend.get();
    auto secondHost = std::make_unique<ResidentAudioService>(
        std::move(secondBackend), secondConnection, serviceName);
    QCOMPARE(secondHost->start(), ServiceStartStatus::Started);
    secondBackendPtr->publish(audioSnapshot(42, 1));
    QTRY_COMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().epoch, quint64(42));
    QVERIFY(client.owner() != firstOwner);
    secondHost->stop();
    secondHost.reset();
    QDBusConnection::disconnectFromBus(secondConnectionName);
    client.stop();
}

QTEST_GUILESS_MAIN(QtAudioTransportTests)
#include "tst_qt_audio_transport.moc"
