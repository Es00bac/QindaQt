// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/voice_preferences/voice_input_preference_gate.h>
#include <qindaqt/services/voice_protocol/voice_types.h>
#include <qindaqt/window_management/qt_command_endpoint.h>
#include <qindaqt/window_management/qt_voice_command_authority.h>
using namespace QindaQt;
using namespace WindowManagement;
namespace {
class SettingsFake final : public Services::SettingsClient::SettingsTransport {
public:
  struct Read {
    quint64 token;
    QString owner;
  };
  QList<Read> reads;
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &) override {
    reads.append({token, owner});
  }
  void commit(quint64, const QString &, const QString &, quint64,
              const QVariantList &) override {}
  void requestActivation() override {}
  void answer(bool enabled) {
    using Services::SettingsProtocol::WireContract;
    const auto read = reads.takeFirst();
    const QVariantMap wire{
        {QLatin1StringView(WireContract::FieldStatus),
         quint32(Services::SettingsProtocol::SettingsWireStatus::Applied)},
        {QLatin1StringView(WireContract::FieldWireSchemaVersion),
         WireContract::WireSchemaVersion},
        {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
         quint32(2)},
        {QLatin1StringView(WireContract::FieldEpoch),
         QStringLiteral("command-epoch")},
        {QLatin1StringView(WireContract::FieldRevision), quint64(1)},
        {QLatin1StringView(WireContract::FieldValues),
         QVariantMap{{QStringLiteral("services.voiceInput"), enabled}}},
        {QLatin1StringView(WireContract::FieldSourceLayers),
         QVariantMap{{QStringLiteral("services.voiceInput"),
                      QStringLiteral("user-overrides")}}},
        {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    Q_EMIT snapshotReceived(read.token, read.owner, wire);
  }
};
struct World final : Scene, Executor {
  int mutations = 0;
  std::optional<ContextSnapshot> capture() const override {
    return ContextSnapshot{"window-a", {}, 1, {}};
  }
  bool current(const ContextSnapshot &) const override { return true; }
  Resolution resolve(const Target &, const ContextSnapshot &) const override {
    return {Status::Accepted, {"window-a", {}}, {}, {}};
  }
  QStringList capabilities() const override {
    return {QStringLiteral("focus")};
  }
  Result execute(const Command &, const ResolvedTarget &target) override {
    ++mutations;
    return {Status::Accepted, {}, {}, {}, target.windowId, target.containerId};
  }
};
QDBusConnection connection(const QString &suffix) {
  return QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                       QUuid::createUuid().toString() + suffix);
}
struct Fixture {
  QDBusConnection server = connection("server"), provider = connection("voice"),
                  stranger = connection("stranger");
  SettingsFake transport;
  Services::SettingsClient::SettingsClient settings{
      transport,
      {QStringLiteral("services.voiceInput")},
      {.requestTimeoutMilliseconds = 500,
       .debounceMilliseconds = 0,
       .retryMilliseconds = {10}}};
  Services::VoicePreferences::VoiceInputPreferenceGate gate{settings};
  bool screenLocked = false;
  QtVoiceCommandAuthority authority{gate, server,
                                    [this] { return screenLocked; }};
  World world;
  QtCommandEndpoint endpoint{authority, world, world};
  Fixture() {
    if (!server.isConnected() ||
        !server.registerService("org.qindaqt.Compositor") ||
        !server.registerObject(QString::fromLatin1(CommandObjectPath),
                               &endpoint,
                               QDBusConnection::ExportScriptableSlots) ||
        !provider.registerService(
            QString::fromLatin1(Services::Voice::kServiceName)) ||
        !settings.start())
      qFatal("private command fixture could not bind");
    QObject::connect(&authority, &QtVoiceCommandAuthority::invalidated,
                     &endpoint, &QtCommandEndpoint::invalidate);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.900"));
  }
  ~Fixture() {
    settings.stop();
    server.unregisterObject(QString::fromLatin1(CommandObjectPath));
    provider.unregisterService(
        QString::fromLatin1(Services::Voice::kServiceName));
    server.unregisterService("org.qindaqt.Compositor");
    QDBusConnection::disconnectFromBus(server.name());
    QDBusConnection::disconnectFromBus(provider.name());
    QDBusConnection::disconnectFromBus(stranger.name());
  }
  QJsonObject call(const QDBusConnection &caller, const QString &method,
                   QVariantList arguments = {}) {
    auto message = QDBusMessage::createMethodCall(
        "org.qindaqt.Compositor", QString::fromLatin1(CommandObjectPath),
        QString::fromLatin1(CommandInterface), method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    const auto pending = caller.asyncCall(message, 2000);
    QDBusPendingCallWatcher watcher(pending);
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&watcher, &QDBusPendingCallWatcher::finished, &loop,
                     &QEventLoop::quit);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(2500);
    if (!watcher.isFinished())
      loop.exec();
    const QDBusPendingReply<QByteArray> reply(pending);
    if (!reply.isFinished() || reply.isError())
      qFatal("private command call failed");
    return QJsonDocument::fromJson(reply.value()).object();
  }
};
const QByteArray Focus =
    R"({"version":1,"operation":"focus","target":{"kind":"current"},"arguments":{}})";
} // namespace
class QtEndpointTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void confirmedOwnerOnlyAndSingleUse() {
    Fixture f;
    QTRY_COMPARE(f.transport.reads.size(), 1);
    QCOMPARE(f.call(f.provider, "BeginCommand").value("status").toString(),
             QStringLiteral("denied"));
    f.transport.answer(true);
    QVERIFY(f.gate.allowed());
    QCOMPARE(f.call(f.stranger, "BeginCommand").value("status").toString(),
             QStringLiteral("denied"));
    const auto capture = f.call(f.provider, "BeginCommand");
    QCOMPARE(capture.value("status").toString(), QStringLiteral("accepted"));
    QVERIFY(capture.contains("capabilities"));
    const auto id = capture.value("contextId").toString();
    QCOMPARE(f.call(f.stranger, "ExecuteCommand", {id, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("denied"));
    QCOMPARE(f.world.mutations, 0);
    QCOMPARE(f.call(f.provider, "ExecuteCommand", {id, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("accepted"));
    QCOMPARE(f.call(f.provider, "ExecuteCommand", {id, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("stale"));
    QCOMPARE(f.world.mutations, 1);
  }
  void ownerLossAndPreferenceLossRevoke() {
    Fixture f;
    QTRY_COMPARE(f.transport.reads.size(), 1);
    f.transport.answer(true);
    const auto old =
        f.call(f.provider, "BeginCommand").value("contextId").toString();
    QSignalSpy revoked(&f.authority, &QtVoiceCommandAuthority::invalidated);
    QVERIFY(f.provider.unregisterService(
        QString::fromLatin1(Services::Voice::kServiceName)));
    QTRY_VERIFY(!revoked.isEmpty());
    QVERIFY(f.stranger.registerService(
        QString::fromLatin1(Services::Voice::kServiceName)));
    QTRY_VERIFY(revoked.size() >= 2);
    QCOMPARE(f.call(f.stranger, "ExecuteCommand", {old, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("stale"));
    const auto fresh =
        f.call(f.stranger, "BeginCommand").value("contextId").toString();
    Q_EMIT f.transport.ownerChanged(QStringLiteral(":1.901"));
    QVERIFY(!f.gate.allowed());
    QCOMPARE(f.call(f.stranger, "ExecuteCommand", {fresh, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("denied"));
    QTRY_COMPARE(f.transport.reads.size(), 1);
    f.transport.answer(true);
    QCOMPARE(f.call(f.stranger, "ExecuteCommand", {fresh, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("stale"));
    QCOMPARE(f.world.mutations, 0);
    f.stranger.unregisterService(
        QString::fromLatin1(Services::Voice::kServiceName));
  }
  void lockAndDirectCallsFailClosed() {
    Fixture f;
    QTRY_COMPARE(f.transport.reads.size(), 1);
    f.transport.answer(true);
    QCOMPARE(QJsonDocument::fromJson(f.endpoint.BeginCommand())
                 .object()
                 .value("status")
                 .toString(),
             QStringLiteral("denied"));
    const auto id =
        f.call(f.provider, "BeginCommand").value("contextId").toString();
    f.screenLocked = true;
    QCOMPARE(f.call(f.provider, "ExecuteCommand", {id, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("denied"));
    f.screenLocked = false;
    QCOMPARE(f.call(f.provider, "ExecuteCommand", {id, Focus})
                 .value("status")
                 .toString(),
             QStringLiteral("stale"));
    QCOMPARE(f.world.mutations, 0);
  }
};
QTEST_GUILESS_MAIN(QtEndpointTest)
#include "tst_qt_endpoint.moc"
