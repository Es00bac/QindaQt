// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_controller.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::RemovableMedia;
class FixtureBackend final : public MediaBackend {
public:
    QVector<Volume> rows;
    QVector<Request> requests;
    bool ready = true;
    QVector<Volume> volumes() const override { return rows; }
    bool available() const override { return ready; }
    QString diagnostic() const override { return QStringLiteral("Disk service unavailable"); }
    QStringList formatTypes() const override { return {QStringLiteral("ext4")}; }
    void refresh() override { Q_EMIT changed(); }
    void execute(const Request &request) override { requests.append(request); }
    void complete(bool success, const QString &path = {}) {
        Q_EMIT finished(requests.constLast().token, success, success ? QStringLiteral("Done") : QStringLiteral("Refused"), path);
    }
};
static Volume fixture(const QString &token = QStringLiteral("attachment-1"))
{
    Volume v;
    v.token = token; v.identity = token; v.path = QStringLiteral("/volume");
    v.device = QStringLiteral("/dev/sdz1"); v.label = QStringLiteral("USB drive");
    v.preferenceKey = QString(64, QLatin1Char('a'));
    v.mountable = true; v.canMountReadOnly = true; v.canFormat = true;
    return v;
}
class MediaPolicyTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void startupAndInsertionPromptOnce() {
        QTemporaryDir dir; MediaPreferences preferences(dir.filePath(QStringLiteral("choices.json")));
        FixtureBackend backend; backend.rows = {fixture()};
        MediaController controller(backend, preferences);
        QSignalSpy prompts(&controller, &MediaController::notificationRequested);
        QTRY_COMPARE(prompts.size(), 1);
        backend.refresh(); QCOMPARE(prompts.size(), 1);
        const auto actions = prompts.constFirst().at(3).toStringList();
        QVERIFY(actions.contains(QStringLiteral("mount")));
        QVERIFY(actions.contains(QStringLiteral("read-only")));
        QVERIFY(actions.contains(QStringLiteral("always")));
        QVERIFY(!actions.contains(QStringLiteral("format")));
        QCOMPARE(backend.requests.size(), 0);
        backend.rows.clear(); backend.refresh();
        backend.rows = {fixture(QStringLiteral("attachment-2"))}; backend.refresh();
        QCOMPARE(prompts.size(), 2);
    }
    void remembersOnlyConfirmedSuccessfulMount() {
        QTemporaryDir dir; const QString path = dir.filePath(QStringLiteral("choices.json"));
        MediaPreferences preferences(path); FixtureBackend backend; backend.rows = {fixture()};
        MediaController controller(backend, preferences, false);
        QSignalSpy opened(&controller, &MediaController::openPathRequested);
        controller.notificationAction(fixture().token, QStringLiteral("always"));
        QCOMPARE(backend.requests.size(), 1); QVERIFY(controller.busy());
        backend.complete(false); QCOMPARE(preferences.mode(fixture().preferenceKey), QStringLiteral("ask"));
        QCOMPARE(opened.size(), 0);
        controller.notificationAction(fixture().token, QStringLiteral("always"));
        backend.complete(true, QStringLiteral("/run/media/test/USB"));
        QCOMPARE(opened.size(), 1);
        QCOMPARE(MediaPreferences(path).mode(fixture().preferenceKey), QStringLiteral("mount"));
    }
    void automaticMountsAreSerializedAndReadOnlyChoiceSurvives() {
        QTemporaryDir dir; MediaPreferences preferences(dir.filePath(QStringLiteral("choices.json")));
        QString error; QVERIFY(preferences.save(fixture().preferenceKey, QStringLiteral("read-only"), &error));
        FixtureBackend backend; backend.rows = {fixture(QStringLiteral("one")), fixture(QStringLiteral("two"))};
        MediaController controller(backend, preferences);
        QTRY_COMPARE(backend.requests.size(), 1);
        QCOMPARE(backend.requests.constFirst().operation, Operation::MountReadOnly);
        backend.complete(true);
        QTRY_COMPARE(backend.requests.size(), 2);
        QCOMPARE(backend.requests.constLast().token, QStringLiteral("two"));
    }
    void ignoredMediaAndReadOnlyDisc() {
        QTemporaryDir dir; MediaPreferences preferences(dir.filePath(QStringLiteral("choices.json")));
        QString error; QVERIFY(preferences.save(fixture().preferenceKey, QStringLiteral("ignore"), &error));
        FixtureBackend backend; backend.rows = {fixture()};
        MediaController controller(backend, preferences);
        QSignalSpy prompts(&controller, &MediaController::notificationRequested);
        QTest::qWait(1); QCOMPARE(prompts.size(), 0); QCOMPARE(backend.requests.size(), 0);
        backend.rows[0].readOnly = true;
        backend.rows[0].optical = true;
        backend.refresh();
        controller.mount(fixture().token);
        QCOMPARE(backend.requests.constLast().operation, Operation::MountReadOnly);
    }
    void formatRequiresCurrentAttachmentUnmountAndExactConfirmation() {
        QTemporaryDir dir; MediaPreferences preferences(dir.filePath(QStringLiteral("choices.json")));
        FixtureBackend backend; backend.rows = {fixture()}; backend.rows[0].mountPath = QStringLiteral("/media/usb");
        MediaController controller(backend, preferences, false);
        controller.requestFormat(fixture().token);
        QVERIFY(!controller.formatTarget().isEmpty());
        controller.confirmFormat(QStringLiteral("ext4"), {}, fixture().device);
        QCOMPARE(backend.requests.size(), 0);
        backend.rows[0].mountPath.clear(); backend.refresh(); controller.requestFormat(fixture().token);
        controller.confirmFormat(QStringLiteral("ext4"), {}, QStringLiteral("/dev/sdz"));
        QCOMPARE(backend.requests.size(), 0);
        controller.requestFormat(fixture().token);
        backend.rows = {fixture(QStringLiteral("new-attachment"))}; backend.refresh();
        QVERIFY(controller.formatTarget().isEmpty());
        controller.confirmFormat(QStringLiteral("ext4"), {}, fixture().device);
        QCOMPARE(backend.requests.size(), 0);
        controller.requestFormat(QStringLiteral("new-attachment"));
        controller.confirmFormat(QStringLiteral("ext4"), QStringLiteral("Files"), fixture().device);
        QCOMPARE(backend.requests.size(), 1);
        QCOMPARE(backend.requests.constFirst().operation, Operation::Format);
        QCOMPARE(backend.requests.constFirst().token, QStringLiteral("new-attachment"));
    }
    void staleNotificationCannotMountOrOpenReplacement() {
        QTemporaryDir dir; MediaPreferences preferences(dir.filePath(QStringLiteral("choices.json")));
        FixtureBackend backend; backend.rows = {fixture()}; MediaController controller(backend, preferences, false);
        controller.mount(fixture().token, false, true);
        QSignalSpy opened(&controller, &MediaController::openPathRequested);
        backend.rows = {fixture(QStringLiteral("replacement"))}; backend.refresh();
        backend.complete(true, QStringLiteral("/media/old")); QCOMPARE(opened.size(), 0);
        controller.notificationAction(fixture().token, QStringLiteral("mount"));
        QCOMPARE(backend.requests.size(), 1);
    }
    void failedPreferenceSaveDoesNotChangePolicy() {
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("blocked"))); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        MediaPreferences preferences(file.fileName() + QStringLiteral("/choices.json"));
        QString error; QVERIFY(!preferences.save(fixture().preferenceKey, QStringLiteral("mount"), &error));
        QVERIFY(!error.isEmpty()); QCOMPARE(preferences.mode(fixture().preferenceKey), QStringLiteral("ask"));
    }
    void projectionHidesInternalRecoveryAndPartitionTable() {
        registerMediaDBusTypes();
        const QString block = QStringLiteral("org.freedesktop.UDisks2.Block");
        const QString drive = QStringLiteral("org.freedesktop.UDisks2.Drive");
        const QString filesystem = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
        const QDBusObjectPath drivePath(QStringLiteral("/drive"));
        ManagedObjects objects{{drivePath, {{drive, {{QStringLiteral("ConnectionBus"), QStringLiteral("usb")},
            {QStringLiteral("MediaAvailable"), true}, {QStringLiteral("Id"), QStringLiteral("usb-serial")}}}}}};
        QVariantMap props{{QStringLiteral("Drive"), QVariant::fromValue(drivePath)},
            {QStringLiteral("PreferredDevice"), QByteArray("/dev/sdz1\0", 10)},
            {QStringLiteral("IdUsage"), QStringLiteral("filesystem")}, {QStringLiteral("IdUUID"), QStringLiteral("uuid")}};
        objects.insert(QDBusObjectPath(QStringLiteral("/data")), {{block, props}, {filesystem, {}}});
        auto hidden = props; hidden.insert(QStringLiteral("HintIgnore"), true);
        objects.insert(QDBusObjectPath(QStringLiteral("/recovery")), {{block, hidden}, {filesystem, {}}});
        auto internal = props; internal.insert(QStringLiteral("HintSystem"), true);
        objects.insert(QDBusObjectPath(QStringLiteral("/internal")), {{block, internal}, {filesystem, {}}});
        objects.insert(QDBusObjectPath(QStringLiteral("/parent")), {{block, props},
            {QStringLiteral("org.freedesktop.UDisks2.PartitionTable"), {}}});
        const auto projected = projectVolumes(objects); QCOMPARE(projected.size(), 1);
        QCOMPARE(projected.constFirst().device, QStringLiteral("/dev/sdz1"));
        QVERIFY(projected.constFirst().mountable); QVERIFY(projected.constFirst().canFormat);
        QCOMPARE(projected.constFirst().preferenceKey.size(), 64);
        objects[drivePath][drive].insert(QStringLiteral("Optical"), true);
        const auto disc = projectVolumes(objects); QVERIFY(disc.constFirst().readOnly);
        QVERIFY(!disc.constFirst().canFormat);
        objects[drivePath][drive].insert(QStringLiteral("MediaAvailable"), false);
        QCOMPARE(projectVolumes(objects).size(), 0);
    }
};
QTEST_GUILESS_MAIN(MediaPolicyTest)
#include "tst_media_policy.moc"
