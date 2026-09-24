// SPDX-License-Identifier: GPL-3.0-or-later
// The bridge end to end on a private bus: the real Settings1 resident
// service, a real purpose-scoped client, the real KConfig writer, and a
// fake org.kde.KWin that counts reconfigure calls.
#include "qindaqt/session/window_management/kwin_reconfigure_requester.h"
#include "qindaqt/session/window_management/kwin_window_management_writer.h"
#include "qindaqt/session/window_management/window_management_apply_state_service.h"
#include "qindaqt/session/window_management/window_management_bridge.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace QindaQt::Session::WindowManagement;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

class FakeKWin final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
public:
    int reconfigures = 0;
public Q_SLOTS:
    void reconfigure() { ++reconfigures; }
};

QString readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll())
                                                            : QString();
}

QDBusMessage callWithEventLoop(const QDBusConnection &bus, const QString &method)
{
    QEventLoop loop;
    const QDBusMessage request = QDBusMessage::createMethodCall(
        QString::fromLatin1(WindowManagementApplyStateService::ServiceName),
        QString::fromLatin1(WindowManagementApplyStateService::ObjectPath),
        QString::fromLatin1(WindowManagementApplyStateService::InterfaceName), method);
    QDBusPendingCallWatcher watcher(bus.asyncCall(request, 2'000));
    QObject::connect(&watcher, &QDBusPendingCallWatcher::finished,
                     &loop, &QEventLoop::quit);
    QTimer::singleShot(2'000, &loop, &QEventLoop::quit);
    if (!watcher.isFinished()) {
        loop.exec();
    }
    return watcher.reply();
}

} // namespace

class WindowManagementBridgeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void confirmedSettingsBecomeKwinrcAndOneReconfigure();
    void dbusApplyStatePublishesAcknowledgementAndRetry();
    void kwinrcWriteFailureNeverClaimsApplied();
    void reconfigureFailureNeverClaimsApplied();
    void compositorOwnerReplacementRequiresANewAcknowledgement();
    void sameSnapshotDoesNotAskForAnotherReconfigure();
};

void WindowManagementBridgeTest::confirmedSettingsBecomeKwinrcAndOneReconfigure()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("wm-service-") + suffix;
    const QString kwinConnection = QStringLiteral("wm-kwin-") + suffix;
    const QString writerConnection = QStringLiteral("wm-writer-") + suffix;
    const QString bridgeConnection = QStringLiteral("wm-bridge-") + suffix;
    auto serviceBus = QDBusConnection::connectToBus(address, serviceConnection);
    auto kwinBus = QDBusConnection::connectToBus(address, kwinConnection);
    auto writerBus = QDBusConnection::connectToBus(address, writerConnection);
    auto bridgeBus = QDBusConnection::connectToBus(address, bridgeConnection);
    QVERIFY(serviceBus.isConnected() && kwinBus.isConnected() && writerBus.isConnected()
            && bridgeBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ResidentSettingsService service(
        serviceBus, *active, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
        directory.filePath(QStringLiteral("user.json")));
    QVERIFY(service.start().ok());

    FakeKWin kwin;
    QVERIFY(kwinBus.registerObject(QStringLiteral("/KWin"), &kwin, QDBusConnection::ExportAllSlots));
    QVERIFY(kwinBus.registerService(QStringLiteral("org.kde.KWin")));

    const QString kwinrcPath = directory.filePath(QStringLiteral("kwinrc"));
    const KWinWindowManagementWriter kwinrc(kwinrcPath);
    DBusKWinReconfigureRequester reconfigure(bridgeBus);
    QtSettingsTransport bridgeTransport(bridgeBus);
    SettingsClient bridgeClient(bridgeTransport, WindowManagementPreferences::scopedKeys(),
                                {.requestTimeoutMilliseconds = 500,
                                 .debounceMilliseconds = 0,
                                 .retryMilliseconds = {10, 20}});
    WindowManagementBridge bridge(bridgeClient, kwinrc, reconfigure);
    bridge.setReconfigureDebounceMilliseconds(20);
    QSignalSpy applied(&bridge, &WindowManagementBridge::applied);
    QVERIFY2(bridgeClient.start(&error), qPrintable(error));

    // Startup: the schema defaults land in a kwinrc that had nothing, so the
    // very first snapshot is a change and costs exactly one reconfigure.
    QTRY_VERIFY_WITH_TIMEOUT(bridge.lastApplied().has_value(), 3'000);
    QVERIFY(*bridge.lastApplied() == WindowManagementPreferences{});
    QCOMPARE(applied.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(kwin.reconfigures, 1, 3'000);
    QVERIFY(readAll(kwinrcPath).contains(QLatin1String("FocusPolicy=ClickToFocus")));

    // The Windows & workspaces route: one ordinary client commits a burst
    // of edits. Every one is decoded as it is confirmed; the reconfigure is
    // debounced to one per burst.
    {
        QtSettingsTransport transport(writerBus);
        SettingsClient writer(transport, WindowManagementPreferences::scopedKeys(),
                              {.requestTimeoutMilliseconds = 500,
                               .debounceMilliseconds = 0,
                               .retryMilliseconds = {10, 20}});
        QVERIFY2(writer.start(&error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(writer.state() == ClientState::Ready, 2'000);
        const QList<QPair<QString, QVariant>> writes{
            {QStringLiteral("windowManagement.focusPolicy"), QStringLiteral("focus-follows-mouse")},
            {QStringLiteral("windowManagement.dockingModifier"), QStringLiteral("alt")},
            {QStringLiteral("windowManagement.snapDistance"), 24},
            {QStringLiteral("windowManagement.closeContainerPolicy"), QStringLiteral("ungroup")},
        };
        for (const auto &[key, value] : writes) {
            QTRY_VERIFY_WITH_TIMEOUT(writer.state() == ClientState::Ready && !writer.writeInFlight(),
                                     2'000);
            QVERIFY2(writer.setUserValue(key, value, &error), qPrintable(error));
            QTRY_VERIFY_WITH_TIMEOUT(!writer.writeInFlight(), 2'000);
            QTRY_VERIFY_WITH_TIMEOUT(writer.snapshot()->values.value(key) == value, 2'000);
        }
    }
    WindowManagementPreferences expected;
    expected.focusPolicy = FocusPolicy::FocusFollowsMouse;
    expected.dockingModifier = DockingModifier::Alt;
    expected.snapDistance = 24;
    expected.closeContainerPolicy = CloseContainerPolicy::Ungroup;
    QTRY_VERIFY_WITH_TIMEOUT(bridge.lastApplied() == std::optional(expected), 3'000);
    QCOMPARE(bridge.rejectedCount(), 0);
    const QString text = readAll(kwinrcPath);
    QVERIFY2(text.contains(QLatin1String("FocusPolicy=FocusFollowsMouse")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("BorderSnapZone=24")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("WindowSnapZone=24")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("DockingModifier=alt")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("CloseContainerPolicy=ungroup")), qPrintable(text));
    QTRY_VERIFY_WITH_TIMEOUT(kwin.reconfigures >= 2, 3'000);
    // Debounced: a burst of four confirmed snapshots asked for far fewer
    // reloads than edits, and the count settles.
    QTest::qWait(200);
    const int settled = kwin.reconfigures;
    QVERIFY2(settled <= 1 + 4, qPrintable(QString::number(settled)));
    QTest::qWait(100);
    QCOMPARE(kwin.reconfigures, settled);
    QCOMPARE(reconfigure.requestCount(), settled);

    service.stop();
    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(kwinConnection);
    QDBusConnection::disconnectFromBus(writerConnection);
    QDBusConnection::disconnectFromBus(bridgeConnection);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

namespace {

class CountingRequester final : public KWinReconfigureRequester
{
public:
    int requests = 0;
    quint64 pendingRequest = 0;
    QString owner = QStringLiteral(":kwin-owner");
    void requestReconfigure(quint64 requestId) override
    {
        ++requests;
        pendingRequest = requestId;
    }
    void finish(const QString &error = {})
    {
        const quint64 completed = pendingRequest;
        pendingRequest = 0;
        Q_EMIT reconfigureFinished(completed, owner, error);
    }
    void loseOwner()
    {
        owner.clear();
        Q_EMIT ownerChanged(owner);
    }
    void replaceOwner()
    {
        owner = QStringLiteral(":kwin-replacement");
        Q_EMIT ownerChanged(owner);
    }
};

// The client's transport seam, driven by hand: no bus, canned snapshots.
class FakeTransport final : public SettingsTransport
{
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        pending.append({token, owner});
    }
    void commit(quint64, const QString &, const QString &, quint64, const QVariantList &) override
    {
    }
    void requestActivation() override {}

    struct Request {
        quint64 token;
        QString owner;
    };
    QList<Request> pending;
};

QVariantMap snapshotWire(quint64 revision, const QVariantMap &values)
{
    QVariantMap layers;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        layers.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap defaultValues()
{
    return {{QStringLiteral("windowManagement.focusPolicy"), QStringLiteral("click")},
            {QStringLiteral("windowManagement.dockingModifier"), QStringLiteral("super")},
            {QStringLiteral("windowManagement.snapDistance"), 12},
            {QStringLiteral("windowManagement.sessionRestore"), true},
            {QStringLiteral("windowManagement.closeContainerPolicy"), QStringLiteral("ask")}};
}

} // namespace

void WindowManagementBridgeTest::dbusApplyStatePublishesAcknowledgementAndRetry()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("wm-state-service-") + suffix;
    const QString readerConnection = QStringLiteral("wm-state-reader-") + suffix;
    auto serviceBus = QDBusConnection::connectToBus(address, serviceConnection);
    auto readerBus = QDBusConnection::connectToBus(address, readerConnection);
    QVERIFY(serviceBus.isConnected() && readerBus.isConnected());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KWinWindowManagementWriter kwinrc(directory.filePath(QStringLiteral("kwinrc")));
    CountingRequester requester;
    FakeTransport transport;
    SettingsClient client(transport, WindowManagementPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    WindowManagementBridge bridge(client, kwinrc, requester);
    bridge.setReconfigureDebounceMilliseconds(0);
    WindowManagementApplyStateService applyState(bridge, serviceBus);
    QString error;
    QVERIFY2(applyState.start(&error), qPrintable(error));
    QString stateReadError;
    const auto readState = [&readerBus, &stateReadError] {
        const QDBusMessage reply = callWithEventLoop(readerBus, QStringLiteral("GetState"));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            stateReadError = QStringLiteral("type %1; %2: ").arg(static_cast<int>(reply.type()))
                + reply.errorName() + QStringLiteral(": ")
                + reply.errorMessage();
            return QVariantMap{};
        }
        return qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    };
    const QVariantMap initialState = readState();
    QVERIFY2(!initialState.isEmpty(), qPrintable(stateReadError));
    QCOMPARE(initialState.value(QStringLiteral("phase")).toString(),
             QStringLiteral("unavailable"));

    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.50"));
    QTRY_COMPARE(transport.pending.size(), 1);
    auto snapshot = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(snapshot.token, snapshot.owner,
                                      snapshotWire(1, defaultValues()));
    QTRY_COMPARE(requester.requests, 1);
    QCOMPARE(readState().value(QStringLiteral("phase")).toString(),
             QStringLiteral("applying"));
    QVERIFY(!bridge.lastApplied().has_value());
    requester.finish();
    QCOMPARE(readState().value(QStringLiteral("phase")).toString(),
             QStringLiteral("applied"));
    QVERIFY(bridge.lastApplied() == std::optional(WindowManagementPreferences{}));

    QVariantMap changed = defaultValues();
    changed[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("alt");
    client.refresh();
    QTRY_COMPARE(transport.pending.size(), 1);
    snapshot = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(snapshot.token, snapshot.owner,
                                      snapshotWire(2, changed));
    QTRY_COMPARE(requester.requests, 2);
    requester.finish(QStringLiteral("KWin did not complete the reload"));
    QCOMPARE(readState().value(QStringLiteral("phase")).toString(),
             QStringLiteral("failed"));
    QVERIFY(readState().value(QStringLiteral("message")).toString()
                .contains(QStringLiteral("did not complete")));

    const QDBusMessage retried = callWithEventLoop(readerBus, QStringLiteral("RetryApply"));
    QCOMPARE(retried.type(), QDBusMessage::ReplyMessage);
    QTRY_COMPARE(requester.requests, 3);
    QCOMPARE(readState().value(QStringLiteral("phase")).toString(),
             QStringLiteral("applying"));
    requester.finish();
    QCOMPARE(readState().value(QStringLiteral("phase")).toString(),
             QStringLiteral("applied"));
    QCOMPARE(qdbus_cast<QVariantMap>(
                 readState().value(QStringLiteral("preferences")))
                 .value(QStringLiteral("windowManagement.dockingModifier")).toString(),
             QStringLiteral("alt"));

    applyState.stop();
    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(readerConnection);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

void WindowManagementBridgeTest::reconfigureFailureNeverClaimsApplied()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KWinWindowManagementWriter kwinrc(directory.filePath(QStringLiteral("kwinrc")));
    CountingRequester requester;
    FakeTransport transport;
    SettingsClient client(transport, WindowManagementPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    WindowManagementBridge bridge(client, kwinrc, requester);
    bridge.setReconfigureDebounceMilliseconds(0);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.20"));
    QTRY_COMPARE(transport.pending.size(), 1);
    const auto request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                       snapshotWire(1, defaultValues()));
    QTRY_COMPARE(requester.requests, 1);
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Applying);
    QVERIFY(!bridge.lastApplied().has_value());

    requester.finish(QStringLiteral("KWin reconfigure timed out"));
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Failed);
    QVERIFY(bridge.applyState().error.contains(QStringLiteral("timed out")));
    QVERIFY(!bridge.lastApplied().has_value());
    QCOMPARE(bridge.appliedCount(), 0);

    // A new explicit baseline retries the same persisted choice. Only its
    // successful KWin reply can move the bridge into Applied.
    client.refresh();
    QTRY_COMPARE(transport.pending.size(), 1);
    const auto retry = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(retry.token, retry.owner,
                                      snapshotWire(2, defaultValues()));
    QTRY_COMPARE(requester.requests, 2);
    requester.finish();
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Applied);
    QVERIFY(bridge.lastApplied() == std::optional(WindowManagementPreferences{}));
    QCOMPARE(bridge.appliedCount(), 1);
}

void WindowManagementBridgeTest::kwinrcWriteFailureNeverClaimsApplied()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("kwinrc"));
    const KWinWindowManagementWriter kwinrc(path);
    CountingRequester requester;
    FakeTransport transport;
    SettingsClient client(transport, WindowManagementPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    WindowManagementBridge bridge(client, kwinrc, requester);
    bridge.setReconfigureDebounceMilliseconds(0);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.25"));
    QTRY_COMPARE(transport.pending.size(), 1);
    auto request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                       snapshotWire(1, defaultValues()));
    QTRY_COMPARE(requester.requests, 1);
    requester.finish();
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Applied);
    QCOMPARE(bridge.applyState().kwinOwner, requester.owner);

    // A directory at the configured file path makes the next write fail
    // reliably even when this test runs with elevated filesystem privileges.
    QVERIFY(QFile::remove(path));
    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("kwinrc")));
    QVariantMap changed = defaultValues();
    changed[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("alt");
    client.refresh();
    QTRY_COMPARE(transport.pending.size(), 1);
    request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(2, changed));

    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Failed);
    QVERIFY(bridge.applyState().error.contains(path));
    QVERIFY(bridge.applyState().kwinOwner.isEmpty());
    QVERIFY(bridge.lastApplied() == std::optional(WindowManagementPreferences{}));
    QCOMPARE(bridge.appliedCount(), 1);
    QCOMPARE(requester.requests, 1);
}

void WindowManagementBridgeTest::compositorOwnerReplacementRequiresANewAcknowledgement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KWinWindowManagementWriter kwinrc(directory.filePath(QStringLiteral("kwinrc")));
    CountingRequester requester;
    FakeTransport transport;
    SettingsClient client(transport, WindowManagementPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    WindowManagementBridge bridge(client, kwinrc, requester);
    bridge.setReconfigureDebounceMilliseconds(0);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.30"));
    QTRY_COMPARE(transport.pending.size(), 1);
    auto request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(1, defaultValues()));
    QTRY_COMPARE(requester.requests, 1);
    requester.finish();
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Applied);

    requester.loseOwner();
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Unavailable);
    QVERIFY(bridge.applyState().error.contains(QStringLiteral("KWin is not running")));
    QVERIFY(!bridge.lastApplied().has_value());
    requester.replaceOwner();
    QTRY_COMPARE(requester.requests, 2);
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Applying);
    QVERIFY(!bridge.lastApplied().has_value());
    requester.finish();
    QCOMPARE(bridge.applyState().phase, WindowManagementApplyPhase::Applied);
    QCOMPARE(bridge.appliedCount(), 2);
}

void WindowManagementBridgeTest::sameSnapshotDoesNotAskForAnotherReconfigure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const KWinWindowManagementWriter kwinrc(directory.filePath(QStringLiteral("kwinrc")));
    CountingRequester requester;
    FakeTransport transport;
    SettingsClient client(transport, WindowManagementPreferences::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    WindowManagementBridge bridge(client, kwinrc, requester);
    bridge.setReconfigureDebounceMilliseconds(0);
    QSignalSpy rejected(&bridge, &WindowManagementBridge::rejected);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(transport.pending.size(), 1);
    auto request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(1, defaultValues()));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QTRY_COMPARE(requester.requests, 1);
    requester.finish();
    QCOMPARE(bridge.appliedCount(), 1);

    // The same values again (a new Settings1 owner re-sending its baseline): decoded,
    // equal, no write and no reconfigure.
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.11"));
    QTRY_COMPARE(transport.pending.size(), 1);
    request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(0, defaultValues()));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QCOMPARE(bridge.appliedCount(), 1);
    QTest::qWait(20);
    QCOMPARE(requester.requests, 1);

    // A snapshot the schema would never produce (a value outside the enum;
    // the client itself already refuses a missing key) is rejected as a
    // whole: the good neighbour value is not applied either, the last good
    // preferences stay, and nothing is written.
    QVariantMap invalid = defaultValues();
    invalid[QStringLiteral("windowManagement.snapDistance")] = 12;
    invalid[QStringLiteral("windowManagement.focusPolicy")] = QStringLiteral("sloppy");
    invalid[QStringLiteral("windowManagement.closeContainerPolicy")] = QStringLiteral("ungroup");
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.12"));
    QTRY_COMPARE(transport.pending.size(), 1);
    request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner, snapshotWire(0, invalid));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QTRY_COMPARE(rejected.size(), 1);
    QCOMPARE(bridge.rejectedCount(), 1);
    QVERIFY2(bridge.lastError().contains(QLatin1String("sloppy")),
             qPrintable(bridge.lastError()));
    QVERIFY(!readAll(kwinrc.path()).contains(QLatin1String("CloseContainerPolicy=ungroup")));
    QVERIFY(bridge.lastApplied() == std::optional(WindowManagementPreferences{}));
    QVERIFY(readAll(kwinrc.path()).contains(QLatin1String("FocusPolicy=ClickToFocus")));
    QCOMPARE(requester.requests, 1);

    // A real change writes and reconfigures once more.
    QVariantMap changed = defaultValues();
    changed[QStringLiteral("windowManagement.dockingModifier")] = QStringLiteral("disabled");
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.13"));
    QTRY_COMPARE(transport.pending.size(), 1);
    request = transport.pending.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner, snapshotWire(0, changed));
    QTRY_VERIFY(client.state() == ClientState::Ready);
    QTRY_COMPARE(requester.requests, 2);
    requester.finish();
    QCOMPARE(bridge.appliedCount(), 2);
    QVERIFY(readAll(kwinrc.path()).contains(QLatin1String("DockingModifier=disabled")));
}

QTEST_GUILESS_MAIN(WindowManagementBridgeTest)
#include "tst_window_management_bridge.moc"
