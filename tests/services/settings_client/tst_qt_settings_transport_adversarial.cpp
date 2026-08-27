// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QProcess>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;

namespace {

class SnapshotObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Settings1")
public:
    bool addExtraField = false;
    bool addExtraArgument = false;
    int delayMilliseconds = 0;
    int snapshotCalls = 0;
    int commitCalls = 0;
    bool decodedCanonicalNull = false;

public Q_SLOTS:
    QVariantMap GetSnapshot(const QStringList &keys)
    {
        ++snapshotCalls;
        QVariantMap values;
        QVariantMap sources;
        for (const auto &key : keys) {
            values.insert(key, false);
            sources.insert(key, QStringLiteral("system-defaults"));
        }
        QVariantMap result{
            {QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("hostile-epoch")},
            {QLatin1StringView(WireContract::FieldRevision), quint64(0)},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
        if (addExtraField) {
            result.insert(QStringLiteral("unbounded-extra-field"), true);
        }
        if (delayMilliseconds > 0 || addExtraArgument) {
            const QDBusConnection replyBus = connection();
            const QDBusMessage request = message();
            QVariantList replyArguments{result};
            if (addExtraArgument) {
                replyArguments.append(true);
            }
            setDelayedReply(true);
            QTimer::singleShot(delayMilliseconds, this,
                               [replyBus, request, replyArguments] {
                const bool sent = replyBus.send(request.createReply(replyArguments));
                Q_UNUSED(sent)
            });
            return {};
        }
        return result;
    }

    QVariantMap CommitUserTransaction(const QString &epoch, quint64 baseRevision,
                                      const QVariantList &operations)
    {
        ++commitCalls;
        Q_UNUSED(epoch)
        const auto operation = operations.size() == 1
            ? decodeBoundedVariantMap(operations.constFirst(), WireContract::OperationFieldCount)
            : std::nullopt;
        QString decodeError;
        const auto value = operation
            ? decodeBoundedJsonValue(
                  operation->value(QLatin1StringView(WireContract::FieldValue)), &decodeError)
            : std::nullopt;
        decodedCanonicalNull = value && value->metaType().id() == QMetaType::Nullptr;
        const QString key = QStringLiteral("services.doNotDisturb");
        return {{QLatin1StringView(WireContract::FieldStatus),
                 quint32(SettingsWireStatus::ValidationFailed)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("hostile-epoch")},
                {QLatin1StringView(WireContract::FieldRevisionBefore), baseRevision},
                {QLatin1StringView(WireContract::FieldRevisionAfter), baseRevision},
                {QLatin1StringView(WireContract::FieldValues), QVariantMap{{key, false}}},
                {QLatin1StringView(WireContract::FieldSourceLayers),
                 QVariantMap{{key, QStringLiteral("system-defaults")}}},
                {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
                {QLatin1StringView(WireContract::FieldMessage),
                 QStringLiteral("fixture rejection")}};
    }
};

bool sendChanged(const QDBusConnection &connection,
                 const QString &epoch,
                 quint64 revision)
{
    auto signal = QDBusMessage::createSignal(
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::SettingsChangedSignal));
    signal << epoch << revision << QStringList{QStringLiteral("services.doNotDisturb")};
    return connection.send(signal);
}

} // namespace

class QtSettingsTransportAdversarialTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void serializesActivationBoundsRepliesAndFencesSubscriptionGeneration();
};

void QtSettingsTransportAdversarialTests::
    serializesActivationBoundsRepliesAndFencesSubscriptionGeneration()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString firstName = QStringLiteral("settings-hostile-first-") + suffix;
    const QString secondName = QStringLiteral("settings-hostile-second-") + suffix;
    const QString clientName = QStringLiteral("settings-hostile-client-") + suffix;
    auto firstBus = QDBusConnection::connectToBus(address, firstName);
    auto secondBus = QDBusConnection::connectToBus(address, secondName);
    auto clientBus = QDBusConnection::connectToBus(address, clientName);
    QVERIFY(firstBus.isConnected());
    QVERIFY(secondBus.isConnected());
    QVERIFY(clientBus.isConnected());

    QtSettingsTransport transport(clientBus);
    QSignalSpy activationFailures(&transport, &SettingsTransport::activationFailed);
    QSignalSpy ownerChanges(&transport, &SettingsTransport::ownerChanged);
    QSignalSpy requestFailures(&transport, &SettingsTransport::requestFailed);
    QSignalSpy snapshots(&transport, &SettingsTransport::snapshotReceived);
    QSignalSpy commits(&transport, &SettingsTransport::commitReceived);
    QSignalSpy invalidations(&transport, &SettingsTransport::settingsChanged);
    QString error;
    QVERIFY2(transport.start(&error), qPrintable(error));
    for (int index = 0; index < 32; ++index) {
        transport.requestActivation();
    }
    QTRY_COMPARE_WITH_TIMEOUT(activationFailures.size(), 1, 2'000);
    QTest::qWait(50);
    QCOMPARE(activationFailures.size(), 1);

    SnapshotObject firstObject;
    firstObject.addExtraField = true;
    QVERIFY(firstBus.registerService(QString::fromLatin1(WireContract::ServiceName)));
    QVERIFY(firstBus.registerObject(QString::fromLatin1(WireContract::ObjectPath),
                                    &firstObject, QDBusConnection::ExportAllSlots));
    QTRY_VERIFY_WITH_TIMEOUT(!ownerChanges.isEmpty(), 2'000);
    const QString firstOwner = firstBus.baseService();
    QCOMPARE(ownerChanges.constLast().constFirst().toString(), firstOwner);

    transport.requestSnapshot(41, firstOwner,
                              {QStringLiteral("services.doNotDisturb")});
    QTRY_COMPARE_WITH_TIMEOUT(requestFailures.size(), 1, 2'000);
    QCOMPARE(snapshots.size(), 0);
    QCOMPARE(requestFailures.constFirst().at(0).toULongLong(), quint64(41));

    firstObject.addExtraField = false;
    transport.requestSnapshot(42, firstOwner,
                              {QStringLiteral("services.doNotDisturb")});
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 2'000);
    QCOMPARE(snapshots.constFirst().at(0).toULongLong(), quint64(42));

    const QVariantMap malformedSet{
        {QLatin1StringView(WireContract::FieldKey),
         QStringLiteral("services.doNotDisturb")},
        {QLatin1StringView(WireContract::FieldKind),
         QLatin1StringView(WireContract::OperationKindSet)},
        {QLatin1StringView(WireContract::FieldValue), QVariant{}}};
    transport.commit(45, firstOwner, QStringLiteral("hostile-epoch"), 0,
                     QVariantList{malformedSet});
    QCOMPARE(requestFailures.size(), 2);
    QCOMPARE(firstObject.commitCalls, 0);

    QVariantMap nullSet = malformedSet;
    nullSet.insert(QLatin1StringView(WireContract::FieldValue),
                   QVariant::fromValue(nullptr));
    transport.commit(46, firstOwner, QStringLiteral("hostile-epoch"), 0,
                     QVariantList{nullSet});
    QTRY_COMPARE_WITH_TIMEOUT(firstObject.commitCalls, 1, 2'000);
    QVERIFY(firstObject.decodedCanonicalNull);
    QTRY_COMPARE_WITH_TIMEOUT(commits.size(), 1, 2'000);

    QVERIFY(sendChanged(firstBus, QStringLiteral("old-epoch"), 1));
    QTRY_COMPARE_WITH_TIMEOUT(invalidations.size(), 1, 2'000);
    QCOMPARE(invalidations.constFirst().at(0).toString(), firstOwner);

    firstObject.addExtraArgument = true;
    transport.requestSnapshot(43, firstOwner,
                              {QStringLiteral("services.doNotDisturb")});
    QTRY_COMPARE_WITH_TIMEOUT(requestFailures.size(), 3, 2'000);
    QCOMPARE(snapshots.size(), 1);
    firstObject.addExtraArgument = false;

    firstObject.delayMilliseconds = 100;
    transport.requestSnapshot(44, firstOwner,
                              {QStringLiteral("services.doNotDisturb")});
    QTRY_COMPARE_WITH_TIMEOUT(firstObject.snapshotCalls, 4, 2'000);
    firstBus.unregisterService(QString::fromLatin1(WireContract::ServiceName));
    SnapshotObject secondObject;
    QVERIFY(secondBus.registerService(QString::fromLatin1(WireContract::ServiceName)));
    QVERIFY(secondBus.registerObject(QString::fromLatin1(WireContract::ObjectPath),
                                     &secondObject, QDBusConnection::ExportAllSlots));
    const QString secondOwner = secondBus.baseService();
    QTRY_VERIFY_WITH_TIMEOUT(!ownerChanges.isEmpty()
                                 && ownerChanges.constLast().constFirst().toString() == secondOwner,
                             2'000);
    bool observedExplicitLoss = false;
    for (const auto &arguments : ownerChanges) {
        observedExplicitLoss = observedExplicitLoss
                               || arguments.constFirst().toString().isEmpty();
    }
    QVERIFY(observedExplicitLoss);
    QTest::qWait(firstObject.delayMilliseconds + 50);
    QCOMPARE(snapshots.size(), 1); // late old-owner reply was generation-fenced
    QCOMPARE(requestFailures.size(), 3);

    // The old connection can still emit a signal after losing the well-known
    // name. The retired exact-owner relay must not relabel it as the replacement.
    QVERIFY(sendChanged(firstBus, QStringLiteral("old-epoch"), 2));
    QTest::qWait(50);
    QCOMPARE(invalidations.size(), 1);
    QVERIFY(sendChanged(secondBus, QStringLiteral("new-epoch"), 1));
    QTRY_COMPARE_WITH_TIMEOUT(invalidations.size(), 2, 2'000);
    QCOMPARE(invalidations.constLast().at(0).toString(), secondOwner);
    QCOMPARE(invalidations.constLast().at(1).toString(), QStringLiteral("new-epoch"));

    transport.stop();
    firstBus.unregisterObject(QString::fromLatin1(WireContract::ObjectPath));
    secondBus.unregisterObject(QString::fromLatin1(WireContract::ObjectPath));
    secondBus.unregisterService(QString::fromLatin1(WireContract::ServiceName));
    QDBusConnection::disconnectFromBus(firstName);
    QDBusConnection::disconnectFromBus(secondName);
    QDBusConnection::disconnectFromBus(clientName);
    daemon.terminate();
    QVERIFY(daemon.waitForFinished());
}

QTEST_GUILESS_MAIN(QtSettingsTransportAdversarialTests)
#include "tst_qt_settings_transport_adversarial.moc"
