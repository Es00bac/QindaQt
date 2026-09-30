// SPDX-License-Identifier: GPL-3.0-or-later
#include "private_bus.h"
#include "qindaqt/services/native_lock_service/native_lock_request.h"
#include "qindaqt/services/native_lock_service/resident_lock_service.h"
#include "qindaqt/services/session_lock_state/native_lock_state_monitor.h"
#include "qindaqt/services/session_lock_state/native_lock_transport.h"
#include <QDBusConnectionInterface>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
using namespace QindaQt::Services::NativeLock;
using namespace QindaQt::Services::SessionLockState;
class Transport final : public NativeLockTransport {
public:
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestOwner(quint64 generation) override {
    Q_EMIT ownerResolved(generation, ":1.10");
  }
  void requestPid(quint64 generation, const QString &owner) override {
    Q_EMIT pidResolved(generation, owner, 4242);
  }
  bool subscribe(const QString &) override { return true; }
  void unsubscribe() override {}
  void requestState(quint64 generation, quint64 serial,
                    const QString &owner) override {
    gen = generation;
    seq = serial;
    name = owner;
  }
  void answer(bool locked, bool protectedPresentation) {
    Q_EMIT stateResolved(gen, seq, name, locked, protectedPresentation);
  }
  quint64 gen = 0, seq = 0;
  QString name;
};
class Request final : public NativeLockRequest {
public:
  bool request() override {
    ++count;
    return send;
  }
  void cancel() override { Q_EMIT completed(RequestResult::Uncertain); }
  void finish(RequestResult result) { Q_EMIT completed(result); }
  int count = 0;
  bool send = true;
};
class Fixture final {
public:
  Fixture()
      : server(bus.connect("server")), client(bus.connect("client")),
        monitor(transport, [this](const QString &, quint64) { return live; }),
        service(server, monitor, request) {
    monitor.start();
  }
  QDBusPendingCall call(const QString &method, const QVariantList &args = {},
                        bool native = false) {
    auto message = QDBusMessage::createMethodCall(
        native ? "org.qindaqt.Lock1" : "org.freedesktop.ScreenSaver",
        native ? "/org/qindaqt/Lock1" : "/ScreenSaver",
        native ? "org.qindaqt.Lock1" : "org.freedesktop.ScreenSaver", method);
    message.setArguments(args);
    return client.asyncCall(message, 3000);
  }
  QVariantMap state() {
    const QDBusPendingReply<QVariantMap> reply(
        waitReply(call("GetState", {}, true)));
    return reply.value();
  }
  PrivateBus bus;
  QDBusConnection server, client;
  Transport transport;
  bool live = true;
  NativeLockStateMonitor monitor;
  Request request;
  ResidentLockService service;
};
class ResidentTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void stateAndProtection() {
    Fixture f;
    QVERIFY(f.service.start());
    const auto unknown = waitReply(f.call("GetActive"));
    QCOMPARE(unknown.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(unknown.errorName(), "org.qindaqt.Lock1.Error.Unavailable");
    QVERIFY(!f.state().value("protected").toBool());
    f.transport.answer(false, false);
    QCOMPARE(QDBusPendingReply<bool>(waitReply(f.call("GetActive"))).value(),
             false);
    auto first = f.state();
    QCOMPARE(first.value("state").toString(), "unlocked");
    QCOMPARE(first.value("version").toUInt(), 1U);
    QVERIFY(!first.value("epoch").toString().isEmpty());
    Q_EMIT f.transport.stateInvalidated(":1.10");
    f.transport.answer(true, false);
    QCOMPARE(f.state().value("state").toString(), "locking");
    QVERIFY(!f.state().value("protected").toBool());
    Q_EMIT f.transport.stateInvalidated(":1.10");
    f.transport.answer(true, true);
    const auto protectedState = f.state();
    QVERIFY(protectedState.value("protected").toBool());
    QVERIFY(protectedState.value("revision").toULongLong() >
            first.value("revision").toULongLong());
    f.live = false;
    QVERIFY(!f.state().value("protected").toBool());
    QCOMPARE(waitReply(f.call("GetActive")).type(), QDBusMessage::ErrorMessage);
  }
  void admissionDoesNotProtect() {
    Fixture f;
    QVERIFY(f.service.start());
    auto pending = f.call("RequestLock", {}, true);
    QTRY_COMPARE(f.request.count, 1);
    f.request.finish(RequestResult::Admitted);
    const QDBusPendingReply<bool> reply(waitReply(pending));
    QVERIFY(!reply.isError());
    QVERIFY(reply.value());
    QCOMPARE(f.state().value("state").toString(), "unknown");
    QVERIFY(!f.state().value("protected").toBool());
  }
  void rejectedAndUncertain() {
    Fixture f;
    QVERIFY(f.service.start());
    auto first = f.call("RequestLock", {}, true);
    QTRY_COMPARE(f.request.count, 1);
    f.request.finish(RequestResult::Rejected);
    QCOMPARE(QDBusPendingReply<bool>(waitReply(first)).value(), false);
    auto second = f.call("Lock");
    QTRY_COMPARE(f.request.count, 2);
    f.request.finish(RequestResult::Rejected);
    QCOMPARE(waitReply(second).errorName(), "org.qindaqt.Lock1.Error.Rejected");
    f.request.send = false;
    QCOMPARE(waitReply(f.call("Lock")).errorName(),
             "org.qindaqt.Lock1.Error.Uncertain");
    QCOMPARE(f.request.count, 3);
  }
  void boundedAndStop() {
    Fixture f;
    QVERIFY(f.service.start());
    const auto epoch = f.state().value("epoch").toString();
    auto first = f.call("Lock");
    QTRY_COMPARE(f.request.count, 1);
    QCOMPARE(waitReply(f.call("SetActive", {true})).errorName(),
             "org.qindaqt.Lock1.Error.Busy");
    QCOMPARE(f.request.count, 1);
    f.service.stop();
    QCOMPARE(waitReply(first).errorName(), "org.qindaqt.Lock1.Error.Uncertain");
    QVERIFY(f.service.start());
    QVERIFY(f.state().value("epoch").toString() != epoch);
    auto second = f.call("SetActive", {true});
    QTRY_COMPARE(f.request.count, 2);
    f.request.finish(RequestResult::Admitted);
    QCOMPARE(waitReply(second).type(), QDBusMessage::ReplyMessage);
  }
  void unsupportedBypassAndInhibit() {
    Fixture f;
    QVERIFY(f.service.start());
    for (const auto &method :
         {QString("SetActive"), QString("SimulateUserActivity"),
          QString("Inhibit"), QString("UnInhibit"), QString("GetActiveTime"),
          QString("GetSessionIdleTime"), QString("Throttle"),
          QString("UnThrottle")}) {
      QVariantList args;
      if (method == "SetActive")
        args << false;
      else if (method == "Inhibit" || method == "Throttle")
        args << "video" << "playing";
      else if (method == "UnInhibit" || method == "UnThrottle")
        args << uint(1);
      QCOMPARE(waitReply(f.call(method, args)).errorName(),
               "org.qindaqt.Lock1.Error.Unsupported");
    }
    QCOMPARE(f.request.count, 0);
  }
  void collisionRollback() {
    Fixture f;
    auto holder = f.bus.connect("holder");
    QVERIFY(holder.registerService("org.freedesktop.ScreenSaver"));
    QVERIFY(!f.service.start());
    QVERIFY(!f.client.interface()
                 ->isServiceRegistered("org.qindaqt.Lock1")
                 .value());
    QCOMPARE(f.client.interface()
                 ->serviceOwner("org.freedesktop.ScreenSaver")
                 .value(),
             holder.baseService());
    QVERIFY(holder.unregisterService("org.freedesktop.ScreenSaver"));
    QVERIFY(f.service.start());
    f.service.stop();
    QVERIFY(!f.client.interface()
                 ->isServiceRegistered("org.freedesktop.ScreenSaver")
                 .value());
  }
};
QTEST_GUILESS_MAIN(ResidentTests)
#include "tst_resident_lock_service.moc"
