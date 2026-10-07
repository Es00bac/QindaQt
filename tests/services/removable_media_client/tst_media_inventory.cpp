// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/removable_media_client/media_client.h>
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include "media_exporter.h"
#include "media_public_projection.h"
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>
#include <utility>

namespace Public = QindaQt::RemovableMedia;
namespace Owner = QindaQt::Apps::RemovableMedia;
class Launcher final : public Public::MediaOwnerLauncher {
public:
    int attempts = 0;
    bool allowed = false;
    bool startOwner() override { ++attempts; return allowed; }
};
class Backend final : public Owner::MediaBackend {
public:
    QVector<Owner::Volume> items;
    bool ready = true;
    quint64 authority = 1;
    int mutations = 0;
    QVector<Owner::Volume> volumes() const override { return items; }
    bool available() const override { return ready; }
    quint64 authorityGeneration() const override { return authority; }
    QString diagnostic() const override { return {}; }
    QStringList formatTypes() const override { return {}; }
    void refresh() override { Q_EMIT changed(); }
    void execute(const Owner::Request &) override { ++mutations; }
};
class WireOwner final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.RemovableMedia1.Devices")
public:
    QByteArray wire;
    int reads = 0;
    bool delay = false;
public Q_SLOTS:
    QString DelayedOwner() {
        setDelayedReply(true);
        const auto bus = connection();
        const auto reply = message().createReply(QVariantList{bus.baseService()});
        QTimer::singleShot(6000, this, [bus, reply] { bus.send(reply); });
        return {};
    }
    QByteArray GetSnapshot() {
        ++reads;
        if (delay) {
            setDelayedReply(true);
            const auto reply = message().createReply(QVariantList{wire});
            const auto bus = connection();
            QTimer::singleShot(100, this, [bus, reply] { bus.send(reply); });
        }
        return wire;
    }
Q_SIGNALS:
    void SnapshotChanged(const QByteArray &wire);
};
class Lookup final : public Public::MediaOwnerLookup {
public:
    explicit Lookup(QDBusConnection bus) : m_bus(std::move(bus)) {}
    int reads = 0, failures = 0;
    QString stallOwner;
    QDBusPendingCall query() override {
        ++reads;
        if (failures > 0) {
            --failures;
            return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("fixture transient lookup")));
        }
        if (!stallOwner.isEmpty()) {
            const auto owner = std::exchange(stallOwner, QString{});
            auto call = QDBusMessage::createMethodCall(owner, QString::fromLatin1(Public::kObjectPath),
                QString::fromLatin1(Public::kInterfaceName), QStringLiteral("DelayedOwner"));
            call.setAutoStartService(false);
            return m_bus.asyncCall(call, 7000);
        }
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
        call.setArguments({QString::fromLatin1(Public::kServiceName)});
        call.setAutoStartService(false);
        return m_bus.asyncCall(call, 5000);
    }
private:
    QDBusConnection m_bus;
};
class ActivationOnly final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.RemovableMedia1")
public:
    int activations = 0;
public Q_SLOTS:
    void Activate() { ++activations; }
};
Public::Snapshot sample(const QString &owner, const QString &epoch = QStringLiteral("epoch1"), quint64 revision = 1)
{
    Public::Snapshot value;
    value.lineage = {owner, epoch, revision};
    value.availability = Public::Availability::Ready;
    Public::VolumeRow row;
    row.driveDisplayId = QStringLiteral("drive1");
    row.volumeDisplayId = QStringLiteral("volume1");
    row.displayName = QStringLiteral("Same label");
    row.kind = QStringLiteral("USB storage");
    row.attachment = {QStringLiteral("attachment1"), 1};
    row.mountState = Public::MountState::Unmounted;
    value.rows.append(row);
    return value;
}
class InventoryTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void cleanup() {
        for (const auto &name : {QStringLiteral("old-media"), QStringLiteral("new-media"),
             QStringLiteral("delayed-media"), QStringLiteral("started-media"),
             QStringLiteral("retry-media"), QStringLiteral("replacement-media")}) {
            QDBusConnection connection(name);
            if (connection.isConnected()) {
                connection.unregisterObject(QString::fromLatin1(Public::kObjectPath));
                connection.unregisterService(QString::fromLatin1(Public::kServiceName));
            }
            QDBusConnection::disconnectFromBus(name);
        }
    }
    void unavailableNeverLaunches();
    void transientAndDeadlineDiscoveryRetry();
    void recoveryDuringInitialLookupRetainsLoading();
    void singleFlightReadAndReplacementFencesLateReply();
    void explicitStartWaitsForReadbackAndTimesOut();
    void oldOwnerUsesActivateWithoutFallback();
    void exactOwnerMalformedAndRevocation();
    void destructionWithPendingRead();
    void ownerProjectionRetainsPartitionsAndReadOnlyTruth();
};
void InventoryTests::unavailableNeverLaunches()
{
    Launcher launcher;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher);
    QSignalSpy changed(&client, &Public::MediaSource::snapshotChanged);
    client.start();
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Unavailable);
    client.refresh();
    QCOMPARE(launcher.attempts, 0);
    QCOMPARE(client.snapshot().rows.size(), 0);
    client.recover();
    QCOMPARE(launcher.attempts, 1);
    QCOMPARE(client.snapshot().availability, Public::Availability::Unavailable);
    QVERIFY(!changed.isEmpty());
}
void InventoryTests::transientAndDeadlineDiscoveryRetry()
{
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("retry-media"));
    WireOwner owner;
    owner.wire = Public::encodeSnapshot(sample(server.baseService())).payload;
    QVERIFY(server.registerObject(QString::fromLatin1(Public::kObjectPath), &owner,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(server.registerService(QString::fromLatin1(Public::kServiceName)));
    Launcher launcher;
    Lookup lookup(QDBusConnection::sessionBus());
    lookup.failures = 1;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher, lookup);
    client.start();
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Unavailable);
    QCOMPARE(lookup.reads, 1);
    for (int i = 0; i < 100; ++i) client.refresh();
    QCOMPARE(lookup.reads, 2);
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Ready);
    QCOMPARE(launcher.attempts, 0);

    Lookup delayedLookup(QDBusConnection::sessionBus());
    delayedLookup.stallOwner = server.baseService();
    Public::MediaClient delayed(QDBusConnection::sessionBus(), launcher, delayedLookup);
    delayed.start();
    for (int i = 0; i < 100; ++i) delayed.refresh();
    QCOMPARE(delayedLookup.reads, 1);
    QTRY_COMPARE_WITH_TIMEOUT(delayed.snapshot().availability, Public::Availability::Unavailable, 6500);
    delayed.refresh();
    QTRY_COMPARE(delayed.snapshot().availability, Public::Availability::Ready);
    QCOMPARE(delayedLookup.reads, 2);
    QTest::qWait(1250);
    QCOMPARE(delayed.snapshot().availability, Public::Availability::Ready);
    QCOMPARE(delayed.snapshot().lineage.owner, server.baseService());
    QCOMPARE(launcher.attempts, 0);
}
void InventoryTests::recoveryDuringInitialLookupRetainsLoading()
{
    Launcher launcher;
    launcher.allowed = true;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher);
    client.start();
    client.recover();
    client.recover();
    QCOMPARE(launcher.attempts, 1);
    QTest::qWait(30);
    QCOMPARE(client.snapshot().availability, Public::Availability::Loading);
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("started-media"));
    WireOwner owner;
    owner.wire = Public::encodeSnapshot(sample(server.baseService())).payload;
    QVERIFY(server.registerObject(QString::fromLatin1(Public::kObjectPath), &owner,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(server.registerService(QString::fromLatin1(Public::kServiceName)));
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Ready);
    QCOMPARE(launcher.attempts, 1);
}
void InventoryTests::singleFlightReadAndReplacementFencesLateReply()
{
    auto first = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("delayed-media"));
    WireOwner old;
    old.delay = true;
    old.wire = Public::encodeSnapshot(sample(first.baseService())).payload;
    QVERIFY(first.registerObject(QString::fromLatin1(Public::kObjectPath), &old,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(first.registerService(QString::fromLatin1(Public::kServiceName)));
    Launcher launcher;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher);
    client.start();
    QTRY_COMPARE(old.reads, 1);
    for (int i = 0; i < 100; ++i) client.refresh();
    QTest::qWait(20);
    QCOMPARE(old.reads, 1);
    QVERIFY(first.unregisterService(QString::fromLatin1(Public::kServiceName)));
    auto replacement = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("replacement-media"));
    WireOwner current;
    auto snapshot = sample(replacement.baseService(), QStringLiteral("replacement"));
    snapshot.rows[0].displayName = QStringLiteral("Replacement media");
    current.wire = Public::encodeSnapshot(snapshot).payload;
    QVERIFY(replacement.registerObject(QString::fromLatin1(Public::kObjectPath), &current,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(replacement.registerService(QString::fromLatin1(Public::kServiceName)));
    QTRY_COMPARE(client.snapshot().lineage.owner, replacement.baseService());
    QTest::qWait(150);
    QCOMPARE(client.snapshot().lineage.epoch, QStringLiteral("replacement"));
    // Even a forged replacement-owner body cannot authenticate the old sender.
    Q_EMIT old.SnapshotChanged(current.wire);
    QTest::qWait(20);
    QCOMPARE(client.snapshot().rows[0].displayName, QStringLiteral("Replacement media"));
    QCOMPARE(launcher.attempts, 0);
}
void InventoryTests::explicitStartWaitsForReadbackAndTimesOut()
{
    Launcher launcher;
    launcher.allowed = true;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher);
    client.start();
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Unavailable);
    client.recover();
    client.recover();
    QCOMPARE(launcher.attempts, 1);
    QCOMPARE(client.snapshot().availability, Public::Availability::Loading);
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("started-media"));
    WireOwner owner;
    owner.wire = Public::encodeSnapshot(sample(server.baseService())).payload;
    QVERIFY(server.registerObject(QString::fromLatin1(Public::kObjectPath), &owner,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(server.registerService(QString::fromLatin1(Public::kServiceName)));
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Ready);
    QCOMPARE(launcher.attempts, 1);
    server.unregisterService(QString::fromLatin1(Public::kServiceName));
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Unavailable);
    client.recover();
    QCOMPARE(launcher.attempts, 2);
    QTRY_COMPARE_WITH_TIMEOUT(client.snapshot().availability, Public::Availability::Unavailable, 6500);
    QVERIFY(client.snapshot().diagnostic.message.contains(QStringLiteral("Try again")));
    QCOMPARE(launcher.attempts, 2);
    server.unregisterObject(QString::fromLatin1(Public::kObjectPath));
    QDBusConnection::disconnectFromBus(QStringLiteral("started-media"));
}
void InventoryTests::oldOwnerUsesActivateWithoutFallback()
{
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("old-media"));
    QVERIFY(server.registerService(QString::fromLatin1(Public::kServiceName)));
    ActivationOnly activation;
    QVERIFY(server.registerObject(QStringLiteral("/org/qindaqt/RemovableMedia1"), &activation,
                                  QDBusConnection::ExportAllSlots));
    Launcher launcher;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher);
    client.start();
    QTRY_COMPARE(client.snapshot().diagnostic.code, Public::DiagnosticCode::Unsupported);
    client.recover();
    QTRY_COMPARE(activation.activations, 1);
    QCOMPARE(launcher.attempts, 0);
    server.unregisterObject(QStringLiteral("/org/qindaqt/RemovableMedia1"));
    server.unregisterService(QString::fromLatin1(Public::kServiceName));
    QDBusConnection::disconnectFromBus(QStringLiteral("old-media"));
}
void InventoryTests::exactOwnerMalformedAndRevocation()
{
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("new-media"));
    QVERIFY(server.registerService(QString::fromLatin1(Public::kServiceName)));
    WireOwner owner;
    owner.wire = Public::encodeSnapshot(sample(server.baseService())).payload;
    QVERIFY(server.registerObject(QString::fromLatin1(Public::kObjectPath), &owner,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    Launcher launcher;
    Public::MediaClient client(QDBusConnection::sessionBus(), launcher);
    client.start();
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Ready);
    QCOMPARE(client.snapshot().rows.size(), 1);
    QCOMPARE(launcher.attempts, 0);
    auto next = sample(server.baseService(), QStringLiteral("epoch1"), 2);
    next.rows.clear();
    Q_EMIT owner.SnapshotChanged(Public::encodeSnapshot(next).payload);
    QTRY_COMPARE(client.snapshot().lineage.revision, 2ULL);
    QCOMPARE(client.snapshot().rows.size(), 0);
    Q_EMIT owner.SnapshotChanged(QByteArray("malformed"));
    QTRY_COMPARE(client.snapshot().diagnostic.code, Public::DiagnosticCode::Invalid);
    QCOMPARE(client.snapshot().rows.size(), 0);
    // Malformed data must retire old ready truth, not silently retain it.
    next = sample(server.baseService(), QStringLiteral("epoch2"), 1);
    Q_EMIT owner.SnapshotChanged(Public::encodeSnapshot(next).payload);
    QTRY_COMPARE(client.snapshot().lineage.epoch, QStringLiteral("epoch2"));
    Q_EMIT owner.SnapshotChanged(Public::encodeSnapshot(sample(server.baseService(), QStringLiteral("epoch1"), 3)).payload);
    QTRY_COMPARE(client.snapshot().diagnostic.code, Public::DiagnosticCode::Stale);
    QCOMPARE(client.snapshot().rows.size(), 0);
    Q_EMIT owner.SnapshotChanged(QByteArray(Public::kMaxSnapshotBytes + 1, 'x'));
    QTRY_COMPARE(client.snapshot().diagnostic.code, Public::DiagnosticCode::Invalid);
    QCOMPARE(client.snapshot().rows.size(), 0);
    server.unregisterService(QString::fromLatin1(Public::kServiceName));
    QTRY_COMPARE(client.snapshot().availability, Public::Availability::Unavailable);
    QCOMPARE(client.snapshot().rows.size(), 0);
    server.unregisterObject(QString::fromLatin1(Public::kObjectPath));
    QDBusConnection::disconnectFromBus(QStringLiteral("new-media"));
}
void InventoryTests::destructionWithPendingRead()
{
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("delayed-media"));
    QVERIFY(server.registerService(QString::fromLatin1(Public::kServiceName)));
    WireOwner owner;
    owner.wire = Public::encodeSnapshot(sample(server.baseService())).payload;
    owner.delay = true;
    QVERIFY(server.registerObject(QString::fromLatin1(Public::kObjectPath), &owner,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    Launcher launcher;
    auto client = std::make_unique<Public::MediaClient>(QDBusConnection::sessionBus(), launcher);
    client->start();
    QTRY_COMPARE(owner.reads, 1);
    client.reset();
    QTest::qWait(150);
    QCOMPARE(launcher.attempts, 0);
    server.unregisterObject(QString::fromLatin1(Public::kObjectPath));
    server.unregisterService(QString::fromLatin1(Public::kServiceName));
    QDBusConnection::disconnectFromBus(QStringLiteral("delayed-media"));
}
void InventoryTests::ownerProjectionRetainsPartitionsAndReadOnlyTruth()
{
    Backend backend;
    Owner::Volume first;
    first.token = QStringLiteral("private-token-a");
    first.identity = QStringLiteral("filesystem-a");
    first.driveIdentity = QStringLiteral("same-physical-drive");
    first.label = QStringLiteral("Same label");
    first.kind = QStringLiteral("USB storage");
    first.partitionNumber = 1;
    first.mountPath = QStringLiteral("/tmp/media-a");
    first.mountRoots = {first.mountPath, QStringLiteral("/tmp/media-a-second")};
    first.readOnlyKnown = true;
    first.readOnly = true;
    auto second = first;
    second.token = QStringLiteral("private-token-b");
    second.identity = QStringLiteral("filesystem-b");
    second.partitionNumber = 2;
    second.mountRoots.clear();
    second.mountPath.clear();
    second.readOnlyKnown = false;
    second.readOnly = false;
    backend.items = {first, second};
    QTemporaryDir config;
    Owner::MediaPreferences prefs(config.filePath(QStringLiteral("choices.json")));
    Owner::MediaController controller(backend, prefs, false);
    Owner::MediaExporter exporter(backend, controller, QDBusConnection::sessionBus());
    auto snapshot = exporter.snapshot();
    QCOMPARE(snapshot.availability, Public::Availability::Ready);
    QCOMPARE(snapshot.rows.size(), 2);
    QCOMPARE(snapshot.rows[0].driveDisplayId, snapshot.rows[1].driveDisplayId);
    QVERIFY(snapshot.rows[0].volumeDisplayId != snapshot.rows[1].volumeDisplayId);
    QVERIFY(snapshot.rows[0].attachment.handle != snapshot.rows[1].attachment.handle);
    QCOMPARE(snapshot.rows[0].mountRoots.size(), 2);
    QCOMPARE(snapshot.rows[0].readOnly, Public::ReadOnlyState::ReadOnly);
    QCOMPARE(snapshot.rows[1].readOnly, Public::ReadOnlyState::Unknown);
    const auto oldEpoch = snapshot.lineage.epoch;
    ++backend.authority;
    backend.refresh();
    QVERIFY(exporter.snapshot().lineage.epoch != oldEpoch);
    backend.items[1].mountPath = QStringLiteral("file:///invalid");
    backend.items[1].mountRoots = {backend.items[1].mountPath};
    backend.refresh();
    QCOMPARE(exporter.snapshot().availability, Public::Availability::Unavailable);
    QCOMPARE(exporter.snapshot().rows.size(), 0);
    QCOMPARE(backend.mutations, 0);
}
QTEST_GUILESS_MAIN(InventoryTests)
#include "tst_media_inventory.moc"
