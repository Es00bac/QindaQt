// SPDX-License-Identifier: GPL-3.0-or-later
// Private compositor scenario: real WindowManagement1 transport and scene,
// synthetic Settings1 consent, and no microphone or owner desktop.
#include <LayerShellQt/Window>
#include <QApplication>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <cstdio>
#include <qindaqt/application_window_management/client.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>
#include <qindaqt/window_management/qt_command_endpoint.h>
using namespace QindaQt;
using namespace WindowManagement;
class Consent final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Settings1")
public:
  bool enabled = false;
  quint64 revision = 1;
  void setEnabled(bool value) {
    enabled = value;
    ++revision;
    Q_EMIT SettingsChanged(QStringLiteral("private-native-command"), revision,
                           {QStringLiteral("services.voiceInput")});
  }
public Q_SLOTS:
  Q_SCRIPTABLE QVariantMap GetSnapshot(const QStringList &) {
    using Services::SettingsProtocol::WireContract;
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(0)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch),
             QStringLiteral("private-native-command")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues),
             QVariantMap{{QStringLiteral("services.voiceInput"), enabled}}},
            {QLatin1StringView(WireContract::FieldSourceLayers),
             QVariantMap{{QStringLiteral("services.voiceInput"),
                          QStringLiteral("user-overrides")}}},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
  }
Q_SIGNALS:
  Q_SCRIPTABLE void SettingsChanged(const QString &epoch, quint64 revision,
                                    const QStringList &keys);
};
class NativeCommands final : public QObject {
  Q_OBJECT
  Consent consent;
  QWidget panel, first, second;
  QDBusConnection bus = QDBusConnection::sessionBus();
  QString firstId, containerId;
  QJsonObject currentTarget{{"kind", "current"}};
  QJsonObject namedTarget;
  QJsonObject invoke(const QDBusConnection &connection, const QString &path,
                     const QString &interface, const QString &method,
                     QVariantList args = {}) {
    auto message = QDBusMessage::createMethodCall("org.qindaqt.Compositor",
                                                  path, interface, method);
    message.setArguments(args);
    message.setAutoStartService(false);
    const auto pending = connection.asyncCall(message, 3000);
    QDBusPendingCallWatcher watcher(pending);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&watcher, &QDBusPendingCallWatcher::finished, &loop,
                     &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(3500);
    if (!watcher.isFinished())
      loop.exec();
    const QDBusPendingReply<QByteArray> reply(pending);
    if (!reply.isFinished() || reply.isError())
      qFatal("private native command transport failed: %s",
             qPrintable(reply.error().message()));
    if (interface == QString::fromLatin1(CommandInterface)) {
      std::printf("COMMAND_RECEIPT %s %s\n", qPrintable(method),
                  reply.value().constData());
      std::fflush(stdout);
    }
    return QJsonDocument::fromJson(reply.value()).object();
  }
  QJsonObject commandCall(const QString &method, QVariantList args = {}) {
    return invoke(bus, QString::fromLatin1(CommandObjectPath),
                  QString::fromLatin1(CommandInterface), method, args);
  }
  QJsonObject control(const QString &method) {
    return invoke(bus, "/org/qindaqt/Compositor", "org.qindaqt.Compositor1",
                  method);
  }
  QJsonObject window(const QString &title) {
    for (const auto &entry : control("Windows").value("windows").toArray()) {
      const auto row = entry.toObject();
      if (row.value("title").toString() == title)
        return row;
    }
    return {};
  }
  QJsonObject container() {
    const auto listing = control("Containers");
    for (const auto &entry : listing.value("containers").toArray())
      if (entry.toObject().value("id").toString() == containerId)
        return entry.toObject();
    return {};
  }
  static QRect rectangle(const QJsonValue &value) {
    const auto r = value.toObject();
    return {qRound(r.value("x").toDouble()), qRound(r.value("y").toDouble()),
            qRound(r.value("width").toDouble()),
            qRound(r.value("height").toDouble())};
  }
  bool frameNear(const QJsonValue &value, const QRectF &expected) const {
    const auto r = value.toObject();
    const QRectF actual(r.value("x").toDouble(), r.value("y").toDouble(),
                        r.value("width").toDouble(),
                        r.value("height").toDouble());
    const qreal tolerance =
        1.0 / qEnvironmentVariable("COMMAND_EXPECTED_SCALE").toDouble() + 1e-6;
    return qAbs(actual.left() - expected.left()) <= tolerance &&
           qAbs(actual.top() - expected.top()) <= tolerance &&
           qAbs(actual.right() - expected.right()) <= tolerance &&
           qAbs(actual.bottom() - expected.bottom()) <= tolerance;
  }
  QJsonObject submit(const QString &op, QJsonObject args = {},
                     QJsonObject target = {}) {
    QTest::qWait(350); // Two admissions per command, below eight/second.
    const auto capture = commandCall("BeginCommand");
    if (capture.value("status") != "accepted")
      return capture;
    const QJsonObject request{
        {"version", 1},
        {"operation", op},
        {"target", target.isEmpty() ? namedTarget : target},
        {"arguments", args}};
    return commandCall("ExecuteCommand",
                       {capture.value("contextId").toString(),
                        QJsonDocument(request).toJson(QJsonDocument::Compact)});
  }
private Q_SLOTS:
  void initTestCase() {
    QVERIFY(bus.isConnected());
    QVERIFY(bus.registerService("org.qindaqt.Settings1"));
    QVERIFY(bus.registerObject("/org/qindaqt/Settings1", &consent,
                               QDBusConnection::ExportScriptableSlots |
                                   QDBusConnection::ExportScriptableSignals));
    QVERIFY(bus.registerService("org.qindaqt.Voice1"));
    panel.setWindowFlags(Qt::FramelessWindowHint |
                         Qt::WindowDoesNotAcceptFocus);
    panel.setWindowTitle("Command fixture reservation");
    panel.resize(1920, 40);
    panel.winId();
    auto *layer = LayerShellQt::Window::get(panel.windowHandle());
    QVERIFY(layer);
    layer->setScope("dock");
    layer->setLayer(LayerShellQt::Window::LayerTop);
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layer->setAnchors(anchors);
    layer->setDesiredSize(QSize(0, 40));
    layer->setExclusiveEdge(LayerShellQt::Window::AnchorTop);
    layer->setExclusiveZone(40);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    panel.show();
    first.setWindowTitle("Native command first");
    first.resize(600, 400);
    first.show();
    QTRY_VERIFY_WITH_TIMEOUT(
        window(first.windowTitle()).value("active").toBool(), 8000);
    firstId = window(first.windowTitle()).value("id").toString();
    QVERIFY(!firstId.isEmpty());
    namedTarget = {{"kind", "window"}, {"id", firstId}};
    const auto expected =
        qEnvironmentVariable("COMMAND_EXPECTED_SCALE").toDouble();
    QTRY_VERIFY(qAbs(first.windowHandle()->devicePixelRatio() - expected) <
                0.01);
    std::printf("ACTUAL_COMMAND_DPR=%.2f\n",
                first.windowHandle()->devicePixelRatio());
    std::fflush(stdout);
  }
  void consentOwnershipAndContext() {
    QCOMPARE(commandCall("BeginCommand").value("status").toString(),
             QStringLiteral("denied"));
    consent.setEnabled(true);
    QTest::qWait(700);
    const auto capture = commandCall("BeginCommand");
    QCOMPARE(capture.value("status").toString(), QStringLiteral("accepted"));
    QCOMPARE(capture.value("windowId").toString(), firstId);
    const auto stranger = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, "native-command-stranger");
    QCOMPARE(invoke(stranger, QString::fromLatin1(CommandObjectPath),
                    QString::fromLatin1(CommandInterface), "BeginCommand")
                 .value("status")
                 .toString(),
             QStringLiteral("denied"));
    const auto wire = QJsonDocument(QJsonObject{{"version", 1},
                                                {"operation", "raise"},
                                                {"target", currentTarget},
                                                {"arguments", QJsonObject{}}})
                          .toJson(QJsonDocument::Compact);
    const auto id = capture.value("contextId").toString();
    QCOMPARE(
        commandCall("ExecuteCommand", {id, wire}).value("status").toString(),
        QStringLiteral("accepted"));
    QCOMPARE(
        commandCall("ExecuteCommand", {id, wire}).value("status").toString(),
        QStringLiteral("stale"));
    QDBusConnection::disconnectFromBus(stranger.name());
  }
  void ordinaryGeometryAndRestoration() {
    const auto original =
        rectangle(window(first.windowTitle()).value("geometry"));
    const auto screen = first.windowHandle()->screen()->geometry();
    const QRect usable(screen.x(), screen.y() + 40, screen.width(),
                       screen.height() - 40);
    const auto inset = regionalFrame(usable, insetRegion(0.9));
    QVERIFY(inset);
    QCOMPARE(submit("maximize").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(
        frameNear(window(first.windowTitle()).value("geometry"), *inset));
    QCOMPARE(submit("restore").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(
        frameNear(window(first.windowTitle()).value("geometry"), original));
    const auto third = regionalFrame(usable, {0, 0, 1.0 / 3, 1});
    QVERIFY(third);
    QCOMPARE(submit("place", {{"region", QJsonArray{0, 0, 1.0 / 3, 1}}})
                 .value("status")
                 .toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(
        frameNear(window(first.windowTitle()).value("geometry"), *third));
    const auto placed =
        window(first.windowTitle()).value("geometry").toObject();
    const QRectF thirdBaseline(
        placed.value("x").toDouble(), placed.value("y").toDouble(),
        placed.value("width").toDouble(), placed.value("height").toDouble());
    QCOMPARE(submit("maximize", {{"fraction", 1}}).value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(
        frameNear(window(first.windowTitle()).value("geometry"), usable));
    QCOMPARE(submit("restore").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(frameNear(window(first.windowTitle()).value("geometry"),
                          thirdBaseline));
    for (int cycle = 0; cycle < 3; ++cycle) {
      QCOMPARE(submit("maximize").value("status").toString(),
               QStringLiteral("accepted"));
      QTRY_VERIFY(
          frameNear(window(first.windowTitle()).value("geometry"), *inset));
      QCOMPARE(submit("restore").value("status").toString(),
               QStringLiteral("accepted"));
      QTRY_VERIFY(frameNear(window(first.windowTitle()).value("geometry"),
                            thirdBaseline));
    }
    QTest::qWait(350);
    const auto captured = commandCall("BeginCommand");
    QCOMPARE(captured.value("status").toString(), QStringLiteral("accepted"));
    const auto beforeResize =
        rectangle(window(first.windowTitle()).value("geometry"));
    first.resize(500, 350);
    QTRY_VERIFY(
        rectangle(window(first.windowTitle()).value("geometry")).size() !=
        beforeResize.size());
    const auto wire = QJsonDocument(QJsonObject{{"version", 1},
                                                {"operation", "close"},
                                                {"target", currentTarget},
                                                {"arguments", QJsonObject{}}})
                          .toJson(QJsonDocument::Compact);
    QCOMPARE(commandCall("ExecuteCommand",
                         {captured.value("contextId").toString(), wire})
                 .value("status")
                 .toString(),
             QStringLiteral("stale"));
    QVERIFY(first.isVisible());
  }
  void realContainerCommandsAndTemporaryStates() {
    second.setWindowTitle("Native command second");
    second.resize(600, 400);
    second.show();
    QTRY_VERIFY(window(second.windowTitle()).value("active").toBool());
    ApplicationWindowManagement::WindowPlacementClient appClient;
    QTRY_VERIFY(appClient.available());
    QSignalSpy done(
        &appClient,
        &ApplicationWindowManagement::WindowPlacementClient::finished);
    QString error;
    QVERIFY(appClient.place(first.windowHandle(), second.windowHandle(),
                            ApplicationWindowManagement::Placement::Tab,
                            &error));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 7000);
    QCOMPARE(
        qvariant_cast<ApplicationWindowManagement::Status>(done.front()[1]),
        ApplicationWindowManagement::Status::Accepted);
    containerId = done.front()[2].toString();
    QVERIFY(!containerId.isEmpty());
    namedTarget = {{"kind", "container"}, {"id", containerId}};
    QCOMPARE(submit("rename", {{"name", "Native voice work"}})
                 .value("status")
                 .toString(),
             QStringLiteral("accepted"));
    QTRY_COMPARE(container().value("displayName").toString(),
                 QStringLiteral("Native voice work"));
    QCOMPARE(submit("color", {{"value", "#336699"}}).value("status").toString(),
             QStringLiteral("accepted"));
    const auto before = rectangle(container().value("outerFrame"));
    QCOMPARE(submit("maximize").value("status").toString(),
             QStringLiteral("accepted"));
    const auto screen = second.windowHandle()->screen()->geometry();
    const auto inset =
        regionalFrame(QRect(screen.x(), screen.y() + 40, screen.width(),
                            screen.height() - 40),
                      insetRegion(0.9));
    QVERIFY(inset);
    QTRY_COMPARE(rectangle(container().value("outerFrame")), *inset);
    QCOMPARE(submit("shade").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(container().value("shaded").toBool());
    QCOMPARE(submit("unshade").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_COMPARE(rectangle(container().value("outerFrame")), *inset);
    QCOMPARE(submit("restore").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_COMPARE(rectangle(container().value("outerFrame")), before);
    QCOMPARE(submit("previous-tab").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(!window(first.windowTitle()).value("minimized").toBool());
    QCOMPARE(submit("next-tab").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(!window(second.windowTitle()).value("minimized").toBool());
    const auto secondId = window(second.windowTitle()).value("id").toString();
    QCOMPARE(submit("detach", {}, {{"kind", "window"}, {"id", secondId}})
                 .value("status")
                 .toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(
        window(second.windowTitle()).value("containerId").toString().isEmpty());
    namedTarget = {{"kind", "window"}, {"id", secondId}};
    QCOMPARE(submit("iconify").value("status").toString(),
             QStringLiteral("accepted"));
    QCOMPARE(submit("uniconify").value("status").toString(),
             QStringLiteral("accepted"));
    QCOMPARE(submit("minimize").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(window(second.windowTitle()).value("minimized").toBool());
    QCOMPARE(submit("restore").value("status").toString(),
             QStringLiteral("accepted"));
    QTRY_VERIFY(!window(second.windowTitle()).value("minimized").toBool());
  }
  void optOutRevokesPriorCapture() {
    QTest::qWait(350);
    const auto capture = commandCall("BeginCommand");
    QCOMPARE(capture.value("status").toString(), QStringLiteral("accepted"));
    consent.setEnabled(false);
    QTest::qWait(700);
    const auto wire = QJsonDocument(QJsonObject{{"version", 1},
                                                {"operation", "close"},
                                                {"target", currentTarget},
                                                {"arguments", QJsonObject{}}})
                          .toJson(QJsonDocument::Compact);
    QCOMPARE(commandCall("ExecuteCommand",
                         {capture.value("contextId").toString(), wire})
                 .value("status")
                 .toString(),
             QStringLiteral("denied"));
    consent.setEnabled(true);
    QTest::qWait(700);
    QCOMPARE(commandCall("ExecuteCommand",
                         {capture.value("contextId").toString(), wire})
                 .value("status")
                 .toString(),
             QStringLiteral("stale"));
    std::puts("NATIVE_COMMANDS_COMPLETED");
    std::fflush(stdout);
  }
  void cleanupTestCase() {
    bus.unregisterObject("/org/qindaqt/Settings1");
    bus.unregisterService("org.qindaqt.Settings1");
    bus.unregisterService("org.qindaqt.Voice1");
  }
};
QTEST_MAIN(NativeCommands)
#include "native_probe.moc"
