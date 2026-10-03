// SPDX-License-Identifier: GPL-3.0-or-later
// Optional cross-repository contract uses the actual exact fork authority,
// private bus and borrowed UI port; no compositor/display acceptance is
// claimed.
#include "global_shortcuts_adaptor.h"
#include "qindaqt/shortcuts/compatibility_endpoint.h"
#include "qindaqt/shortcuts/input_dispatcher.h"
#include "tests/apps/settings/input/support/private_bus.h"
#include <QDBusPendingCallWatcher>
#include <QEventLoop>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <qindaqt/services/shortcuts_client/transport.h>
using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::Shortcuts;
namespace {
QDBusMessage invoke(const QDBusConnection &bus, const QString &path,
                    const QString &iface, const QString &member,
                    QVariantList args = {}) {
  auto call = QDBusMessage::createMethodCall(
      "org.freedesktop.impl.portal.desktop.qindaqt", path, iface, member);
  call.setArguments(args);
  QDBusPendingCallWatcher pending(bus.asyncCall(call, 2500));
  QEventLoop loop;
  QObject::connect(&pending, &QDBusPendingCallWatcher::finished, &loop,
                   &QEventLoop::quit);
  if (!pending.isFinished())
    loop.exec();
  return pending.reply();
}
class EditingUi final : public ShortcutUi {
public:
  bool allowed = true;
  RequestResponse response = RequestResponse::Success;
  QJsonObject last;
  int cancelled = 0;
  bool admitted() const override { return allowed; }
  void ask(RequestToken token, const QJsonObject &frame) override {
    last = frame;
    QTimer::singleShot(0, this, [this, token, frame] {
      Q_EMIT completed(token, response,
                       {{"shortcuts", frame.value("shortcuts")}});
    });
  }
  void cancel(RequestToken) override { ++cancelled; }
};
} // namespace
class NativeBridgeTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void settingsUsesActualNativeAuthority() {
    QindaQt::Tests::PrivateBus bus;
    QVERIFY(bus.start());
    KWin::Shortcuts::Registry registry;
    KWin::Shortcuts::CompatibilityEndpoint authority(bus.connection, &registry);
    QVERIFY(authority.start("org.kde.kglobalaccel"));
    QtShortcutTransport native(bus.connection);
    QVERIFY(native.available());
    Binding binding;
    binding.component = "shell";
    binding.action = "launcher";
    binding.description = "Show launcher";
    binding.keys = {QKeySequence("Meta+Space")};
    QString error;
    QVERIFY2(native.registerBinding(binding, false, &error), qPrintable(error));
    QTemporaryDir dir;
    QindaQt::Apps::SettingsInput::QtShortcutPort settings(bus.connection,
                                                          dir.path());
    const auto rows = settings.actions(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().active, binding.keys);
    QVERIFY(settings.setShortcuts("shell", "launcher",
                                  {QKeySequence("Ctrl+K, Ctrl+C")}, &error));
    QCOMPARE(settings.actions(&error).first().active,
             QList<QKeySequence>{QKeySequence("Ctrl+K, Ctrl+C")});
    Binding conflict;
    conflict.component = "other";
    conflict.action = "occupied";
    conflict.description = "Occupied";
    conflict.keys = {QKeySequence("Meta+L")};
    QVERIFY(native.registerBinding(conflict, false, &error));
    QVERIFY(!settings.setShortcuts("shell", "launcher", conflict.keys, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(settings.actions(nullptr).first().componentUnique,
             QStringLiteral("other"));
  }
  void portalBindsDispatchesAndClosesActualNativeActions() {
    QindaQt::Tests::PrivateBus bus;
    QVERIFY(bus.start());
    const QString frontendName = bus.name + QStringLiteral("-frontend");
    auto frontend = QDBusConnection::connectToBus(bus.address, frontendName);
    QVERIFY(frontend.isConnected());
    struct DisconnectFrontend {
      QString name;
      ~DisconnectFrontend() { QDBusConnection::disconnectFromBus(name); }
    } disconnectFrontend{frontendName};
    QVERIFY(frontend.registerService("org.freedesktop.portal.Desktop"));
    KWin::Shortcuts::Registry registry;
    KWin::Shortcuts::CompatibilityEndpoint authority(bus.connection, &registry);
    QVERIFY(authority.start("org.kde.kglobalaccel"));
    QtShortcutTransport native(bus.connection);
    QObject host;
    RequestRegistry requests(bus.connection);
    EditingUi ui;
    GlobalShortcutsAdaptor adaptor(host, requests, ui, native, bus.connection);
    QVERIFY(bus.connection.registerService(
        "org.freedesktop.impl.portal.desktop.qindaqt"));
    QVERIFY(bus.connection.registerObject("/org/freedesktop/portal/desktop",
                                          &host,
                                          QDBusConnection::ExportAdaptors));
    QString caller = frontend.baseService().mid(1);
    caller.replace('.', '_');
    const QString root = "/org/freedesktop/portal/desktop",
                  iface = "org.freedesktop.impl.portal.GlobalShortcuts";
    const QString session = root + "/session/" + caller + "/shortcuts";
    const QString handle = root + "/request/" + caller + "/";
    const auto created =
        invoke(frontend, root, iface, "CreateSession",
               {QVariant::fromValue(QDBusObjectPath(handle + "create")),
                QVariant::fromValue(QDBusObjectPath(session)), "example.app",
                QVariantMap{{"handle_token", "create"},
                            {"session_handle_token", "shortcuts"}}});
    QCOMPARE(created.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(created.arguments().first().toUInt(), 0U);
    PortalShortcuts offered{
        {"capture",
         {{"description", "Capture"}, {"preferred_trigger", "CTRL+F12"}}}};
    const auto bound =
        invoke(frontend, root, iface, "BindShortcuts",
               {QVariant::fromValue(QDBusObjectPath(handle + "bind")),
                QVariant::fromValue(QDBusObjectPath(session)),
                QVariant::fromValue(offered), "",
                QVariantMap{{"handle_token", "bind"}}});
    QCOMPARE(bound.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(bound.arguments().first().toUInt(), 0U);
    QCOMPARE(registry.bindings().size(), 1);
    QCOMPARE(registry.bindings().first().shortcuts,
             QList<QKeySequence>{QKeySequence("Ctrl+F12")});
    QSignalSpy activated(&adaptor, &GlobalShortcutsAdaptor::Activated),
        deactivated(&adaptor, &GlobalShortcutsAdaptor::Deactivated);
    KWin::Shortcuts::InputDispatcher input(
        registry, [&](const KWin::Shortcuts::Binding &binding,
                      KWin::Shortcuts::ShortcutEvent event) {
          authority.notifyShortcut(binding, event, 0);
        });
    QVERIFY(input.key(int(Qt::ControlModifier) | Qt::Key_F12,
                      KWin::Shortcuts::ShortcutEvent::Pressed, false, false));
    input.key(Qt::Key_F12, KWin::Shortcuts::ShortcutEvent::Released, false,
              false);
    QTRY_COMPARE(activated.count(), 1);
    QTRY_COMPARE(deactivated.count(), 1);
    QVERIFY(activated.first().at(2).toULongLong() > 0);
    const auto listed =
        invoke(frontend, root, iface, "ListShortcuts",
               {QVariant::fromValue(QDBusObjectPath(handle + "list")),
                QVariant::fromValue(QDBusObjectPath(session))});
    QCOMPARE(listed.arguments().first().toUInt(), 0U);
    const auto closed = invoke(frontend, session,
                               "org.freedesktop.impl.portal.Session", "Close");
    QCOMPARE(closed.type(), QDBusMessage::ReplyMessage);
    QVERIFY(registry.bindings().isEmpty());

    // A cancelled editor acquires no native binding; frontend loss revokes
    // an already-bound session even when the caller process remains alive.
    const QString cancelledSession = session + "_cancelled";
    const auto cancelledCreate = invoke(
        frontend, root, iface, "CreateSession",
        {QVariant::fromValue(QDBusObjectPath(handle + "create_cancelled")),
         QVariant::fromValue(QDBusObjectPath(cancelledSession)), "example.app",
         QVariantMap{{"session_handle_token", "shortcuts_cancelled"}}});
    QCOMPARE(cancelledCreate.arguments().first().toUInt(), 0U);
    ui.response = RequestResponse::Cancelled;
    const auto cancelledBind =
        invoke(frontend, root, iface, "BindShortcuts",
               {QVariant::fromValue(QDBusObjectPath(handle + "bind_cancelled")),
                QVariant::fromValue(QDBusObjectPath(cancelledSession)),
                QVariant::fromValue(offered), "", QVariantMap{}});
    QCOMPARE(cancelledBind.arguments().first().toUInt(), 1U);
    QVERIFY(registry.bindings().isEmpty());
    ui.response = RequestResponse::Success;
    const auto rebound =
        invoke(frontend, root, iface, "BindShortcuts",
               {QVariant::fromValue(QDBusObjectPath(handle + "rebind")),
                QVariant::fromValue(QDBusObjectPath(cancelledSession)),
                QVariant::fromValue(offered), "", QVariantMap{}});
    QCOMPARE(rebound.arguments().first().toUInt(), 0U);
    QCOMPARE(registry.bindings().size(), 1);
    QVERIFY(frontend.unregisterService("org.freedesktop.portal.Desktop"));
    QTRY_VERIFY(registry.bindings().isEmpty());
  }
};
QTEST_GUILESS_MAIN(NativeBridgeTest)
#include "tst_native_bridge.moc"
