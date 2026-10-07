// SPDX-License-Identifier: GPL-3.0-or-later
#include "udisks_backend.h"
#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusMetaType>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Apps::RemovableMedia;
static const QString Service = QStringLiteral("org.freedesktop.UDisks2");
static const QString Root = QStringLiteral("/org/freedesktop/UDisks2");
static const QString Block = Service + QStringLiteral(".Block");
static const QString Fs = Service + QStringLiteral(".Filesystem");
static const QString Drive = Service + QStringLiteral(".Drive");
static const QString DataPath = Root + QStringLiteral("/block_devices/sdz1");
static const QString HiddenPath = Root + QStringLiteral("/block_devices/sdz2");
static const QString DrivePath = Root + QStringLiteral("/drives/USB");

struct FormatAvailability { bool available = true; QString program; };
Q_DECLARE_METATYPE(FormatAvailability)
static QDBusArgument &operator<<(QDBusArgument &arg, const FormatAvailability &v)
{ arg.beginStructure(); arg << v.available << v.program; arg.endStructure(); return arg; }
static const QDBusArgument &operator>>(const QDBusArgument &arg, FormatAvailability &v)
{ arg.beginStructure(); arg >> v.available >> v.program; arg.endStructure(); return arg; }

class ObjectManager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")
public:
    ManagedObjects objects;
public Q_SLOTS:
    ManagedObjects GetManagedObjects() const { return objects; }
Q_SIGNALS:
    void InterfacesAdded(const QDBusObjectPath &path, const Interfaces &interfaces);
    void InterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
};
class Manager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UDisks2.Manager")
public Q_SLOTS:
    FormatAvailability CanFormat(const QString &) const { return {}; }
};
class TestFilesystem final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UDisks2.Filesystem")
public:
    TestFilesystem(ObjectManager &manager, QString path, QStringList &calls)
        : inventory(manager), objectPath(std::move(path)), log(calls) {}
    ObjectManager &inventory;
    QString objectPath;
    QStringList &log;
    QVariantMap mountOptions;
    bool busy = false, delayed = false, delayedUnmount = false;
    bool omitMountReadback = false, retainMountOnUnmount = false, authorizationCancelled = false;
    QDBusMessage pending;
public Q_SLOTS:
    QString Mount(const QVariantMap &options) {
        log.append(QStringLiteral("mount:") + objectPath); mountOptions = options;
        if (delayed) { setDelayedReply(true); pending = message(); return {}; }
        if (authorizationCancelled) {
            sendErrorReply(Service + QStringLiteral(".Error.NotAuthorizedDismissed"), QStringLiteral("fixture cancel"));
            return {};
        }
        const QString path = QStringLiteral("/run/media/test/USB");
        if (!omitMountReadback) inventory.objects[QDBusObjectPath(objectPath)][Fs].insert(QStringLiteral("MountPoints"),
            QVariant::fromValue(QList<QByteArray>{path.toUtf8() + '\0'}));
        return path;
    }
    void Unmount(const QVariantMap &options) {
        log.append(QStringLiteral("unmount:") + objectPath);
        QVERIFY(!options.value(QStringLiteral("force")).toBool());
        if (delayedUnmount) { setDelayedReply(true); pending = message(); return; }
        if (busy) { sendErrorReply(Service + QStringLiteral(".Error.DeviceBusy"), QStringLiteral("Busy")); return; }
        if (!retainMountOnUnmount) inventory.objects[QDBusObjectPath(objectPath)][Fs].insert(QStringLiteral("MountPoints"),
            QVariant::fromValue(QList<QByteArray>{}));
    }
};
class TestFormat final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UDisks2.Block")
public:
    explicit TestFormat(TestFilesystem &parent) : QDBusAbstractAdaptor(&parent), endpoint(parent) {}
    TestFilesystem &endpoint;
    QString type;
    QVariantMap options;
public Q_SLOTS:
    void Format(const QString &filesystem, const QVariantMap &values) {
        type = filesystem; options = values; endpoint.log.append(QStringLiteral("format"));
    }
};
class TestDrive final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UDisks2.Drive")
public:
    explicit TestDrive(QStringList &calls) : log(calls) {}
    QStringList &log;
    bool delayed = false;
    QDBusMessage pending;
public Q_SLOTS:
    void PowerOff(const QVariantMap &) {
        log.append(QStringLiteral("power-off"));
        if (delayed) { setDelayedReply(true); pending = message(); }
    }
    void Eject(const QVariantMap &) { log.append(QStringLiteral("eject")); }
};
class TestEncrypted final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UDisks2.Encrypted")
public:
    TestEncrypted(ObjectManager &manager, QStringList &calls) : inventory(manager), log(calls) {}
    ObjectManager &inventory;
    QStringList &log;
public Q_SLOTS:
    void Lock(const QVariantMap &) {
        log.append(QStringLiteral("lock"));
        inventory.objects.remove(QDBusObjectPath(DataPath));
        inventory.objects[QDBusObjectPath(HiddenPath)][Service + QStringLiteral(".Encrypted")]
            .insert(QStringLiteral("CleartextDevice"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))));
        Q_EMIT inventory.InterfacesRemoved(QDBusObjectPath(DataPath), {Block, Fs});
    }
};

class MediaUDisksTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { registerMediaDBusTypes(); qDBusRegisterMetaType<FormatAvailability>(); }
    void inventoryMountReadOnlyAndFormat() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &formatter,
                        ObjectManager &manager, QDBusConnection &, QStringList &log) {
            QTRY_VERIFY(backend.available()); QTRY_COMPARE(backend.formatTypes().size(), 3);
            QCOMPARE(backend.volumes().size(), 1);
            auto v = backend.volumes().constFirst();
            QSignalSpy results(&backend, &MediaBackend::finished);
            Request mount; mount.token = v.token; mount.operation = Operation::MountReadOnly;
            // The fixture represents inherently read-only media, so the real
            // backend need not inspect a fictional host mountpoint.
            manager.objects[QDBusObjectPath(DataPath)][Block].insert(QStringLiteral("ReadOnly"), true);
            backend.execute(mount); QTRY_COMPARE(results.size(), 1);
            QVERIFY(results.constLast().at(1).toBool());
            QCOMPARE(data.mountOptions.value(QStringLiteral("options")).toString(), QStringLiteral("ro"));
            Request format; format.token = v.token; format.operation = Operation::Format; format.filesystem = QStringLiteral("ext4");
            backend.execute(format); QTRY_COMPARE(results.size(), 2);
            QVERIFY(!results.constLast().at(1).toBool()); QVERIFY(formatter.type.isEmpty());
            manager.objects[QDBusObjectPath(DataPath)][Block].insert(QStringLiteral("ReadOnly"), false);
            manager.objects[QDBusObjectPath(DataPath)][Fs].insert(QStringLiteral("MountPoints"), QVariant::fromValue(QList<QByteArray>{}));
            backend.refresh(); QTRY_VERIFY(!backend.volumes().constFirst().readOnly);
            format.token = backend.volumes().constFirst().token;
            backend.execute(format); QTRY_COMPARE(results.size(), 3);
            QVERIFY(results.constLast().at(1).toBool()); QCOMPARE(formatter.type, QStringLiteral("ext4"));
            QVERIFY(!formatter.options.value(QStringLiteral("tear-down")).toBool());
            QVERIFY(log.contains(QStringLiteral("format")));
        });
    }
    void busyHiddenSiblingPreventsPowerOff() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &, TestFilesystem &hidden, TestFormat &,
                        ObjectManager &manager, QDBusConnection &, QStringList &log) {
            mountBoth(manager); hidden.busy = true;
            QTRY_VERIFY(backend.available()); QSignalSpy results(&backend, &MediaBackend::finished);
            Request remove; remove.token = backend.volumes().constFirst().token; remove.operation = Operation::Remove;
            backend.execute(remove); QTRY_COMPARE(results.size(), 1);
            QVERIFY(!results.constLast().at(1).toBool());
            QVERIFY(results.constLast().at(2).toString().contains(QStringLiteral("busy")));
            QVERIFY(!log.contains(QStringLiteral("power-off")));
            QVERIFY(log.contains(QStringLiteral("unmount:") + HiddenPath));
        });
    }
    void allSiblingVolumesUnmountBeforePowerOff() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &, TestFilesystem &, TestFormat &,
                        ObjectManager &manager, QDBusConnection &, QStringList &log) {
            mountBoth(manager); QTRY_VERIFY(backend.available());
            QSignalSpy results(&backend, &MediaBackend::finished);
            Request remove; remove.token = backend.volumes().constFirst().token; remove.operation = Operation::Remove;
            backend.execute(remove); QTRY_COMPARE(results.size(), 1);
            QVERIFY(results.constLast().at(1).toBool());
            QCOMPARE(log, QStringList({QStringLiteral("unmount:") + DataPath,
                QStringLiteral("unmount:") + HiddenPath, QStringLiteral("power-off")}));
        });
    }
    void successfulMountReplyWithoutReadbackIsUncertain() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &,
                        ObjectManager &, QDBusConnection &, QStringList &log) {
            QTRY_VERIFY(backend.available()); data.omitMountReadback = true;
            QVector<BackendCompletion> results;
            connect(&backend, &MediaBackend::operationCompleted, this, [&](const BackendCompletion &r) { results.append(r); });
            Request mount; mount.token = backend.volumes().constFirst().token;
            backend.execute(mount); QTRY_COMPARE(results.size(), 1);
            QCOMPARE(results.first().status, QindaQt::RemovableMedia::OperationStatus::Uncertain);
            QVERIFY(backend.volumes().constFirst().mountRoots.isEmpty());
            QCOMPARE(log.count(QStringLiteral("mount:") + DataPath), 1);
        });
    }
    void successfulUnmountReplyWithRootsRemainingIsUncertain() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &,
                        ObjectManager &manager, QDBusConnection &, QStringList &log) {
            mountBoth(manager); QTRY_VERIFY(backend.available()); data.retainMountOnUnmount = true;
            QVector<BackendCompletion> results;
            connect(&backend, &MediaBackend::operationCompleted, this, [&](const BackendCompletion &r) { results.append(r); });
            Request unmount; unmount.token = backend.volumes().constFirst().token; unmount.operation = Operation::Unmount;
            backend.execute(unmount); QTRY_COMPARE(results.size(), 1);
            QCOMPARE(results.first().status, QindaQt::RemovableMedia::OperationStatus::Uncertain);
            QVERIFY(!backend.volumes().constFirst().mountRoots.isEmpty());
            QCOMPARE(log.count(QStringLiteral("unmount:") + DataPath), 1);
        });
    }
    void hiddenSiblingReadbackPreventsPowerOffAfterSuccessfulReply() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &, TestFilesystem &hidden, TestFormat &,
                        ObjectManager &manager, QDBusConnection &, QStringList &log) {
            mountBoth(manager); QTRY_VERIFY(backend.available()); hidden.retainMountOnUnmount = true;
            QVector<BackendCompletion> results;
            connect(&backend, &MediaBackend::operationCompleted, this, [&](const BackendCompletion &r) { results.append(r); });
            Request remove; remove.token = backend.volumes().constFirst().token; remove.operation = Operation::Remove;
            backend.execute(remove); QTRY_COMPARE(results.size(), 1);
            QCOMPARE(results.first().status, QindaQt::RemovableMedia::OperationStatus::Uncertain);
            QCOMPARE(results.first().removalMode, QindaQt::RemovableMedia::RemovalMode::None);
            QVERIFY(!log.contains(QStringLiteral("power-off")));
        });
    }
    void cancelledAuthorizationHasTypedOutcome() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &,
                        ObjectManager &, QDBusConnection &, QStringList &) {
            QTRY_VERIFY(backend.available()); data.authorizationCancelled = true;
            QVector<BackendCompletion> results;
            connect(&backend, &MediaBackend::operationCompleted, this, [&](const BackendCompletion &r) { results.append(r); });
            Request mount; mount.token = backend.volumes().constFirst().token;
            backend.execute(mount); QTRY_COMPARE(results.size(), 1);
            QCOMPARE(results.first().status, QindaQt::RemovableMedia::OperationStatus::Cancelled);
        });
    }
    void lateFinalReplyCannotCertifyAnnouncedReplacement() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &, TestFilesystem &, TestFormat &,
                        ObjectManager &manager, QDBusConnection &server, QStringList &) {
            mountBoth(manager); QTRY_VERIFY(backend.available());
            auto *drive = qobject_cast<TestDrive *>(server.objectRegisteredAt(DrivePath)); QVERIFY(drive);
            drive->delayed = true;
            QVector<BackendCompletion> results;
            connect(&backend, &MediaBackend::operationCompleted, this, [&](const BackendCompletion &r) { results.append(r); });
            Request remove; remove.token = backend.volumes().constFirst().token; remove.operation = Operation::Remove;
            backend.execute(remove); QTRY_VERIFY(drive->pending.type() == QDBusMessage::MethodCallMessage);
            auto replacement = manager.objects.value(QDBusObjectPath(DrivePath));
            replacement[Drive][QStringLiteral("Id")] = QStringLiteral("new-drive-before-debounce");
            manager.objects[QDBusObjectPath(DrivePath)] = replacement;
            Q_EMIT manager.InterfacesRemoved(QDBusObjectPath(DrivePath), {Drive});
            Q_EMIT manager.InterfacesAdded(QDBusObjectPath(DrivePath), replacement);
            QTRY_COMPARE(backend.volumes().size(), 0);
            QVERIFY(server.send(drive->pending.createReply()));
            QTRY_COMPARE(results.size(), 1);
            QCOMPARE(results.first().status, QindaQt::RemovableMedia::OperationStatus::Uncertain);
            QCOMPARE(results.first().removalMode, QindaQt::RemovableMedia::RemovalMode::None);
        });
    }
    void removedAttachmentRejectsStaleFormatAndNotificationToken() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &, TestFilesystem &, TestFormat &formatter,
                        ObjectManager &manager, QDBusConnection &, QStringList &) {
            QTRY_VERIFY(backend.available()); const QString oldToken = backend.volumes().constFirst().token;
            manager.objects.remove(QDBusObjectPath(DataPath));
            Q_EMIT manager.InterfacesRemoved(QDBusObjectPath(DataPath), {Block, Fs});
            QTRY_COMPARE(backend.volumes().size(), 0);
            QSignalSpy results(&backend, &MediaBackend::finished);
            Request format; format.token = oldToken; format.operation = Operation::Format; format.filesystem = QStringLiteral("ext4");
            backend.execute(format); QCOMPARE(results.size(), 1);
            QVERIFY(!results.constLast().at(1).toBool()); QVERIFY(formatter.type.isEmpty());
        });
    }
    void lockingSelectedCleartextAllowsPhysicalRemoval() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &, TestFilesystem &, TestFormat &,
                        ObjectManager &manager, QDBusConnection &server, QStringList &log) {
            const QString encrypted = Service + QStringLiteral(".Encrypted");
            manager.objects[QDBusObjectPath(DataPath)][Block][QStringLiteral("Drive")]
                = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")));
            manager.objects[QDBusObjectPath(DataPath)][Block][QStringLiteral("CryptoBackingDevice")]
                = QVariant::fromValue(QDBusObjectPath(HiddenPath));
            manager.objects[QDBusObjectPath(HiddenPath)].remove(Fs);
            manager.objects[QDBusObjectPath(HiddenPath)][encrypted]
                .insert(QStringLiteral("CleartextDevice"), QVariant::fromValue(QDBusObjectPath(DataPath)));
            manager.objects[QDBusObjectPath(DataPath)][Fs][QStringLiteral("MountPoints")]
                = QVariant::fromValue(QList<QByteArray>{QByteArray("/media/fixture\0", 15)});
            TestEncrypted endpoint(manager, log); server.unregisterObject(HiddenPath);
            QVERIFY(server.registerObject(HiddenPath, &endpoint, QDBusConnection::ExportAllSlots));
            QTRY_VERIFY(backend.available()); QCOMPARE(backend.volumes().size(), 1);
            QSignalSpy results(&backend, &MediaBackend::finished);
            Request remove; remove.token = backend.volumes().constFirst().token; remove.operation = Operation::Remove;
            backend.execute(remove); QTRY_COMPARE(results.size(), 1);
            QVERIFY(results.constLast().at(1).toBool());
            QCOMPARE(log, QStringList({QStringLiteral("unmount:") + DataPath,
                QStringLiteral("lock"), QStringLiteral("power-off")}));
            server.unregisterObject(HiddenPath);
        });
    }
    void delayedSafeRemovalMustNotPowerOffReplacement() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &,
                        ObjectManager &manager, QDBusConnection &server, QStringList &log) {
            mountBoth(manager);
            QTRY_VERIFY(backend.available());
            const QString oldToken = backend.volumes().constFirst().token;
            data.delayedUnmount = true;
            QSignalSpy results(&backend, &MediaBackend::finished);
            Request remove; remove.token = oldToken; remove.operation = Operation::Remove;
            backend.execute(remove);
            QTRY_VERIFY(data.pending.type() == QDBusMessage::MethodCallMessage);
            const Interfaces originalDrive = manager.objects.take(QDBusObjectPath(DrivePath));
            const Interfaces originalData = manager.objects.take(QDBusObjectPath(DataPath));
            Q_EMIT manager.InterfacesRemoved(QDBusObjectPath(DrivePath), {Drive});
            Q_EMIT manager.InterfacesRemoved(QDBusObjectPath(DataPath), {Block, Fs});
            QTRY_COMPARE(backend.volumes().size(), 0);
            auto replacementDrive = originalDrive;
            replacementDrive[Drive][QStringLiteral("Id")] = QStringLiteral("OTHER-DRIVE");
            replacementDrive[Drive][QStringLiteral("TimeDetected")] = quint64(1234);
            auto replacementData = originalData;
            replacementData[Block][QStringLiteral("IdUUID")] = QStringLiteral("REPLACEMENT");
            manager.objects.insert(QDBusObjectPath(DrivePath), replacementDrive);
            manager.objects.insert(QDBusObjectPath(DataPath), replacementData);
            Q_EMIT manager.InterfacesAdded(QDBusObjectPath(DrivePath), replacementDrive);
            Q_EMIT manager.InterfacesAdded(QDBusObjectPath(DataPath), replacementData);
            backend.refresh();
            QTRY_COMPARE(backend.volumes().size(), 1);
            QVERIFY(backend.volumes().constFirst().token != oldToken);
            QVERIFY(server.send(data.pending.createReply()));
            QTRY_COMPARE(results.size(), 1);
            QVERIFY(!results.constLast().at(1).toBool());
            QVERIFY2(!log.contains(QStringLiteral("power-off")),
                     "Safe-removal continuation powered off a replacement drive at the reused path");
        });
    }
    void lateReadOnlyReplyMustNotUnmountReplacement() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &,
                        ObjectManager &manager, QDBusConnection &server, QStringList &log) {
            QTRY_VERIFY(backend.available());
            const QString oldToken = backend.volumes().constFirst().token;
            data.delayed = true;
            QSignalSpy results(&backend, &MediaBackend::finished);
            Request mount; mount.token = oldToken; mount.operation = Operation::MountReadOnly;
            backend.execute(mount);
            QTRY_VERIFY(data.pending.type() == QDBusMessage::MethodCallMessage);
            const Interfaces original = manager.objects.take(QDBusObjectPath(DataPath));
            Q_EMIT manager.InterfacesRemoved(QDBusObjectPath(DataPath), {Block, Fs});
            QTRY_COMPARE(backend.volumes().size(), 0);
            auto replacement = original;
            replacement[Block][QStringLiteral("IdUUID")] = QStringLiteral("REPLACEMENT");
            replacement[Fs][QStringLiteral("MountPoints")] = QVariant::fromValue(QList<QByteArray>{QByteArray("/tmp\0", 5)});
            manager.objects.insert(QDBusObjectPath(DataPath), replacement);
            Q_EMIT manager.InterfacesAdded(QDBusObjectPath(DataPath), replacement);
            backend.refresh();
            QTRY_COMPARE(backend.volumes().size(), 1);
            QVERIFY(backend.volumes().constFirst().token != oldToken);
            QVERIFY(server.send(data.pending.createReply(QVariantList{QStringLiteral("/tmp")})));
            QTRY_COMPARE(results.size(), 1);
            QVERIFY(!results.constLast().at(1).toBool());
            QVERIFY2(!log.contains(QStringLiteral("unmount:") + DataPath),
                     "Late read-only reply dispatched Unmount to a replacement attachment at the reused path");
        });
    }
    void serviceLossCancelsDelayedReplyWithoutReplay() {
        withFixture([&](UDisksBackend &backend, TestFilesystem &data, TestFilesystem &, TestFormat &,
                        ObjectManager &, QDBusConnection &server, QStringList &log) {
            QTRY_VERIFY(backend.available()); const QString oldToken = backend.volumes().constFirst().token;
            data.delayed = true; QSignalSpy results(&backend, &MediaBackend::finished);
            Request mount; mount.token = oldToken; backend.execute(mount);
            QTRY_VERIFY(data.pending.type() == QDBusMessage::MethodCallMessage);
            QVERIFY(server.unregisterService(Service));
            QTRY_COMPARE(results.size(), 1); QVERIFY(!results.constLast().at(1).toBool());
            QVERIFY(!backend.available()); QVERIFY(server.registerService(Service));
            QTRY_VERIFY(backend.available()); QVERIFY(backend.volumes().constFirst().token != oldToken);
            server.send(data.pending.createReply(QVariantList{QStringLiteral("/media/stale")}));
            QTest::qWait(20); QCOMPARE(results.size(), 1); QCOMPARE(log.size(), 1);
        });
    }
private:
    static void mountBoth(ObjectManager &manager) {
        for (const QString &path : {DataPath, HiddenPath})
            manager.objects[QDBusObjectPath(path)][Fs].insert(QStringLiteral("MountPoints"),
                QVariant::fromValue(QList<QByteArray>{QByteArray("/media/fixture\0", 15)}));
    }
    template<class Function> void withFixture(Function function) {
        auto server = QDBusConnection::connectToBus(QString::fromUtf8(qgetenv("DBUS_SESSION_BUS_ADDRESS")), QStringLiteral("media-test-server"));
        QVERIFY(server.isConnected()); QVERIFY(server.registerService(Service));
        QStringList log; ObjectManager objects; Manager manager; TestDrive drive(log);
        QVariantMap block{{QStringLiteral("Drive"), QVariant::fromValue(QDBusObjectPath(DrivePath))},
            {QStringLiteral("IdUsage"), QStringLiteral("filesystem")}, {QStringLiteral("IdUUID"), QStringLiteral("UUID")},
            {QStringLiteral("Device"), QByteArray("/dev/sdz1\0", 10)}, {QStringLiteral("Size"), quint64(1024)}};
        objects.objects.insert(QDBusObjectPath(DrivePath), {{Drive, {{QStringLiteral("ConnectionBus"), QStringLiteral("usb")},
            {QStringLiteral("MediaAvailable"), true}, {QStringLiteral("CanPowerOff"), true}, {QStringLiteral("Id"), QStringLiteral("USB")}}}});
        objects.objects.insert(QDBusObjectPath(DataPath), {{Block, block}, {Fs, {}}});
        block.insert(QStringLiteral("HintIgnore"), true);
        objects.objects.insert(QDBusObjectPath(HiddenPath), {{Block, block}, {Fs, {}}});
        TestFilesystem data(objects, DataPath, log), hidden(objects, HiddenPath, log); TestFormat formatter(data);
        const auto flags = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAdaptors;
        QVERIFY(server.registerObject(Root, &objects, flags));
        QVERIFY(server.registerObject(Root + QStringLiteral("/Manager"), &manager, flags));
        QVERIFY(server.registerObject(DrivePath, &drive, flags));
        QVERIFY(server.registerObject(DataPath, &data, flags)); QVERIFY(server.registerObject(HiddenPath, &hidden, flags));
        { UDisksBackend backend(QDBusConnection::sessionBus()); function(backend, data, hidden, formatter, objects, server, log); }
        server.unregisterService(Service); server.unregisterObject(Root, QDBusConnection::UnregisterTree);
        QDBusConnection::disconnectFromBus(QStringLiteral("media-test-server"));
    }
};
QTEST_GUILESS_MAIN(MediaUDisksTest)
#include "tst_media_udisks.moc"
