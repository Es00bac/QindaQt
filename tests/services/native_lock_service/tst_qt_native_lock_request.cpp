// SPDX-License-Identifier: GPL-3.0-or-later
#include "private_bus.h"
#include "qindaqt/compositor_names/compositor_names.h"
#include "qindaqt/platform/compositor_attachment/compositor_attachment.h"
#include "qindaqt/services/native_lock_service/qt_native_lock_request.h"
#include "qindaqt/services/native_lock_service/resident_lock_service.h"
#include "qindaqt/services/session_lock_state/native_lock_state_monitor.h"
#include "qindaqt/services/session_lock_state/qt_native_lock_transport.h"
#include <QDBusConnectionInterface>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusVirtualObject>
#include <QFile>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
namespace CompositorNames = QindaQt::CompositorNames;
using namespace QindaQt::Services::NativeLock;
using namespace QindaQt::Platform::Compositor;

class Backend final : public QDBusVirtualObject {
public:
  Backend(QDBusConnection connection, QDBusConnection spoof) : bus(std::move(connection)), attacker(std::move(spoof)) {}
  QString introspect(const QString &) const override {
    return QStringLiteral(
        "<interface name=\"%1\"><method name=\"RequestLockWithReceipt\"><arg direction=\"in\" type=\"s\"/></method>"
        "<method name=\"RequestStateWithReceipt\"><arg direction=\"in\" type=\"s\"/></method>"
        "<signal name=\"lockAdmissionReceipt\"><arg type=\"s\"/><arg type=\"b\"/></signal>"
        "<signal name=\"stateReceipt\"><arg type=\"s\"/><arg type=\"b\"/><arg type=\"b\"/></signal></interface>")
        .arg(QString(CompositorNames::nativeLockInterface));
  }
  bool handleMessage(const QDBusMessage &message,
                     const QDBusConnection &) override {
    if (message.interface() ==
            QStringLiteral("org.freedesktop.DBus.Properties") &&
        message.member() == QStringLiteral("GetAll")) {
      QVariantMap state{{QStringLiteral("Locked"), locked},
                        {QStringLiteral("Protected"), protectedPresentation}};
      bus.send(message.createReply(QVariant::fromValue(state)));
      return true;
    }
    if (message.interface() != QString(CompositorNames::nativeLockInterface))
      return false;
    if (message.member() == QStringLiteral("RequestStateWithReceipt")) {
      auto reply = message.createReply();
      bus.send(reply);
      auto receipt = QDBusMessage::createTargetedSignal(
          message.service(), QString(CompositorNames::nativeLockPath),
          QString(CompositorNames::nativeLockInterface),
          QStringLiteral("stateReceipt"));
      receipt << message.arguments().first().toString() << locked
              << protectedPresentation;
      bus.send(receipt);
      return true;
    }
    if (message.member() != QStringLiteral("RequestLockWithReceipt"))
      return false;
    calls.append(message);
    std::puts("CALL");
    std::fflush(stdout);
    return true;
  }
  void reply(const QVariant &value) {
    if (calls.isEmpty())
      return;
    const auto call = calls.takeFirst();
    const QString mode = value.metaType() == QMetaType::fromType<QString>()
                             ? value.toString()
                             : QString();
    if (mode == QStringLiteral("missing")) {
      bus.send(call.createReply());
      return;
    }
    if (mode == QStringLiteral("wrong-type")) {
      bus.send(call.createReply(QStringLiteral("true")));
      return;
    }
    auto receipt = QDBusMessage::createTargetedSignal(
        call.service(), QString(CompositorNames::nativeLockPath),
        QString(CompositorNames::nativeLockInterface),
        QStringLiteral("lockAdmissionReceipt"));
    receipt << call.arguments().first().toString()
            << (value.metaType() == QMetaType::fromType<bool>() &&
                value.toBool());
    if (mode == QStringLiteral("wrong-nonce")) {
      auto args = receipt.arguments();
      args[0] = QStringLiteral("00000000000000000000000000000000");
      receipt.setArguments(args);
    }
    bus.send(receipt);
    if (mode == QStringLiteral("duplicate"))
      bus.send(receipt);
    bus.send(call.createReply());
  }
  void forgedSignal() {
    if (calls.isEmpty())
      return;
    const auto call = calls.takeFirst();
    auto receipt = QDBusMessage::createTargetedSignal(
        call.service(), QString(CompositorNames::nativeLockPath),
        QString(CompositorNames::nativeLockInterface),
        QStringLiteral("lockAdmissionReceipt"));
    receipt << call.arguments().first().toString() << true;
    attacker.send(receipt);
    bus.send(call.createReply());
  }
  void state(bool active, bool protectedState) {
    locked = active;
    protectedPresentation = protectedState;
    for (const auto &member : {QStringLiteral("lockedChanged"),
                               QStringLiteral("protectedChanged")}) {
      auto signal = QDBusMessage::createSignal(
          QString(CompositorNames::nativeLockPath),
          QString(CompositorNames::nativeLockInterface), member);
      signal << (member == "lockedChanged" ? locked : protectedPresentation);
      bus.send(signal);
    }
  }
  QDBusConnection bus;
  QDBusConnection attacker;
  bool locked = false, protectedPresentation = false;
  QList<QDBusMessage> calls;
};
int backendMain(const QString &address, const QString &path) {
  const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  sockaddr_un socketAddress{};
  socketAddress.sun_family = AF_UNIX;
  const auto bytes = QFile::encodeName(path);
  if (bytes.size() >= qsizetype(sizeof(socketAddress.sun_path)))
    return 2;
  std::memcpy(socketAddress.sun_path, bytes.constData(),
              size_t(bytes.size()) + 1);
  if (fd < 0 ||
      bind(fd, reinterpret_cast<sockaddr *>(&socketAddress),
           sizeof(socketAddress)) != 0 ||
      listen(fd, 16) != 0)
    return 3;
  QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  auto peer = QDBusConnection::connectToBus(address, "backend");
  auto attacker = QDBusConnection::connectToBus(address, "attacker");
  Backend backend(peer, attacker);
  if (!peer.registerService(QString(CompositorNames::service)) ||
      !peer.registerVirtualObject(QString(CompositorNames::nativeLockPath),
                                  &backend))
    return 4;
  const auto owner = peer.baseService().toUtf8();
  std::puts(owner.constData());
  std::fflush(stdout);
  QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
  QByteArray pending;
  QList<QDBusConnection> additional;
  QObject::connect(&input, &QSocketNotifier::activated, &input, [&] {
    char commandBytes[4096];
    const auto count = read(STDIN_FILENO, commandBytes, sizeof(commandBytes));
    if (count <= 0) {
      QCoreApplication::quit();
      return;
    }
    pending.append(commandBytes, qsizetype(count));
    while (pending.contains('\n')) {
      const auto pos = pending.indexOf('\n');
      const auto command = pending.left(pos);
      pending.remove(0, pos + 1);
      if (command == "true")
        backend.reply(true);
      else if (command == "false")
        backend.reply(false);
      else if (command == "wrong-type")
        backend.reply(QStringLiteral("wrong-type"));
      else if (command == "duplicate")
        backend.reply(QStringLiteral("duplicate"));
      else if (command == "wrong-nonce")
        backend.reply(QStringLiteral("wrong-nonce"));
      else if (command == "missing")
        backend.reply(QStringLiteral("missing"));
      else if (command == "forged-signal")
        backend.forgedSignal();
      else if (command == "state locking")
        backend.state(true, false);
      else if (command == "state protected")
        backend.state(true, true);
      else if (command == "state unlocked")
        backend.state(false, false);
      else if (command == "lose") {
        peer.unregisterService(QString(CompositorNames::service));
        std::puts("LOST");
        std::fflush(stdout);
      } else if (command == "replace") {
        peer.unregisterService(QString(CompositorNames::service));
        auto replacement = QDBusConnection::connectToBus(
            address, QStringLiteral("replacement"));
        replacement.registerService(QString(CompositorNames::service));
        additional.append(replacement);
        std::puts("REPLACED");
        std::fflush(stdout);
      } else if (command.startsWith("join ")) {
        auto other = QDBusConnection::connectToBus(
            QString::fromUtf8(command.mid(5)),
            QStringLiteral("additional-%1").arg(additional.size()));
        if (other.registerService(QString(CompositorNames::service)))
          additional.append(other);
        const auto line =
            (QStringLiteral("JOIN ") + other.baseService()).toUtf8();
        std::puts(line.constData());
        std::fflush(stdout);
      }
    }
  });
  const int result = QCoreApplication::exec();
  close(fd);
  return result;
}
class Peer final : public QObject {
public:
  Peer() {
    poll.setInterval(5);
    connect(&poll, &QTimer::timeout, this, [this] { drain(); });
  }
  ~Peer() {
    process.terminate();
    process.waitForFinished(5000);
  }
  bool start(const QString &address, const QString &path) {
    process.start(QCoreApplication::applicationFilePath(),
                  {"--backend", address, path});
    if (!process.waitForStarted() || !process.waitForReadyRead())
      return false;
    owner = QString::fromUtf8(process.readLine()).trimmed();
    poll.start();
    return owner.startsWith(QLatin1Char(':'));
  }
  void drain() {
    while (process.canReadLine()) {
      const auto line = process.readLine().trimmed();
      if (line == "CALL")
        calls.append(QDBusMessage{});
      else if (line == "LOST")
        lost = true;
      else if (line == "REPLACED")
        replaced = true;
      else if (line.startsWith("JOIN "))
        joined = QString::fromUtf8(line.mid(5));
    }
  }
  void reply(const QVariant &value) {
    if (!calls.isEmpty())
      calls.removeFirst();
    if (value.metaType() == QMetaType::fromType<bool>()) {
      process.write(value.toBool() ? "true\n" : "false\n");
    } else {
      process.write(value.toString().toUtf8() + "\n");
    }
  }
  bool replaceService() {
    process.write("replace\n");
    QElapsedTimer timer;
    timer.start();
    while (!replaced && timer.elapsed() < 2000) {
      process.waitForReadyRead(10);
      drain();
    }
    return replaced;
  }
  bool unregisterService(const QString &) {
    process.write("lose\n");
    QElapsedTimer timer;
    timer.start();
    while (!lost && timer.elapsed() < 2000) {
      process.waitForReadyRead(10);
      drain();
    }
    return lost;
  }
  bool join(const QString &address) {
    process.write("join " + address.toUtf8() + "\n");
    QElapsedTimer timer;
    timer.start();
    while (joined.isEmpty() && timer.elapsed() < 2000) {
      process.waitForReadyRead(10);
      drain();
    }
    return !joined.isEmpty();
  }
  QString baseService() const { return owner; }
  QProcess process;
  QTimer poll;
  QString owner, joined;
  QList<QDBusMessage> calls;
  bool lost = false;
  bool replaced = false;
};
class Fixture final {
public:
  Fixture()
      : session(bus.connect("session")), client(bus.connect("client")),
        attachment(client, runtime.path(),
                   [this](const QString &owner) {
                     return selected && owner == session.baseService();
                   }),
        request(client, attachment), backend(peer) {
    path =
        runtime.filePath(QString(CompositorNames::waylandSocketPrefix) + "0");
    ready = peer.start(bus.address, path);
  }
  ~Fixture() { attachment.revoke(); }
  bool attach() {
    return attachment.attach(session.baseService(),
                             QString(CompositorNames::waylandSocketPrefix) +
                                 "0");
  }
  PrivateBus bus;
  QDBusConnection session, client;
  QTemporaryDir runtime;
  bool selected = true;
  CompositorAttachment attachment;
  QtNativeLockRequest request;
  Peer peer;
  Peer &backend;
  QString path;
  bool ready = false;
};
class RequestTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void strictReply_data() {
    QTest::addColumn<QVariant>("reply");
    QTest::addColumn<RequestResult>("expected");
    QTest::newRow("admitted") << QVariant(true) << RequestResult::Admitted;
    QTest::newRow("rejected") << QVariant(false) << RequestResult::Rejected;
    QTest::newRow("wrong-type") << QVariant("wrong-type") << RequestResult::Uncertain;
    QTest::newRow("wrong-nonce") << QVariant("wrong-nonce") << RequestResult::Uncertain;
    QTest::newRow("forged-signal") << QVariant("forged-signal") << RequestResult::Uncertain;
    QTest::newRow("duplicate") << QVariant("duplicate") << RequestResult::Uncertain;
    QTest::newRow("missing-receipt") << QVariant("missing") << RequestResult::Uncertain;
  }
  void strictReply() {
    QFETCH(QVariant, reply);
    QFETCH(RequestResult, expected);
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    QSignalSpy result(&f.request, &NativeLockRequest::completed);
    QVERIFY(f.request.request());
    QVERIFY(!f.request.request());
    QTRY_COMPARE(f.backend.calls.size(), 1);
    f.backend.reply(reply);
    QTRY_COMPARE(result.size(), 1);
    QCOMPARE(result.first().first().value<RequestResult>(), expected);
  }
  void noAttachmentOrRevokedAdmission() {
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(!f.request.request());
    QVERIFY(f.attach());
    f.selected = false;
    QVERIFY(!f.request.request());
    QVERIFY(f.backend.calls.isEmpty());
  }
  void anotherBusCannotReuseOwnerPid() {
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    PrivateBus other;
    auto session = other.connect("session");
    auto client = other.connect("client");
    QVERIFY(f.peer.join(other.address));
    QCOMPARE(f.peer.joined, f.peer.baseService());
    QCOMPARE(client.interface()->servicePid(f.peer.joined).value(),
             uint(f.peer.process.processId()));
    QVERIFY(!f.attachment.sameBus(client));
    QVERIFY(f.attachment.sameBus(f.client));
    QtNativeLockRequest wrong(client, f.attachment);
    QVERIFY(!wrong.request());
    QVERIFY(f.backend.calls.isEmpty());
  }
  void cancellationFencesLateReply() {
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    QSignalSpy result(&f.request, &NativeLockRequest::completed);
    QVERIFY(f.request.request());
    QTRY_COMPARE(f.backend.calls.size(), 1);
    f.attachment.revoke();
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first().first().value<RequestResult>(),
             RequestResult::Uncertain);
    QVERIFY(f.attach());
    QVERIFY(f.request.request());
    QTRY_COMPARE(f.backend.calls.size(), 2);
    f.backend.reply(true);
    QTest::qWait(10);
    QCOMPARE(result.size(), 1);
    f.backend.reply(false);
    QTRY_COMPARE(result.size(), 2);
    QCOMPARE(result.last().first().value<RequestResult>(),
             RequestResult::Rejected);
  }
  void ownerReplacementCannotCompleteOldReceipt() {
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    QSignalSpy result(&f.request, &NativeLockRequest::completed);
    QVERIFY(f.request.request());
    QTRY_COMPARE(f.backend.calls.size(), 1);
    QVERIFY(f.peer.replaceService());
    f.backend.reply(true);
    QTRY_COMPARE(result.size(), 1);
    QCOMPARE(result.first().first().value<RequestResult>(),
             RequestResult::Uncertain);
  }
  void actualOwnerLossBeforeReply() {
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    QSignalSpy result(&f.request, &NativeLockRequest::completed);
    QVERIFY(f.request.request());
    QTRY_COMPARE(f.backend.calls.size(), 1);
    QVERIFY(f.peer.unregisterService(QString(CompositorNames::service)));
    f.backend.reply(true);
    QTRY_COMPARE(result.size(), 1);
    QCOMPARE(result.first().first().value<RequestResult>(),
             RequestResult::Uncertain);
    QVERIFY(!f.request.request());
  }
  void realCompositionProtectsOnlyActualNativeSnapshot() {
    using namespace QindaQt::Services::SessionLockState;
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    QtNativeLockTransport transport(f.client);
    NativeLockStateMonitor monitor(
        transport, [&](const QString &owner, quint64 pid) {
          const auto identity = f.attachment.identity();
          return identity && identity->compositorOwner == owner &&
                 identity->compositorPid == pid;
        });
    ResidentLockService facade(f.client, monitor, f.request);
    QVERIFY(monitor.start());
    QVERIFY(facade.start());
    QTRY_COMPARE(monitor.state(), LockState::Unlocked);
    const auto state = [&] {
      auto call = QDBusMessage::createMethodCall(
          "org.qindaqt.Lock1", "/org/qindaqt/Lock1", "org.qindaqt.Lock1",
          "GetState");
      const QDBusPendingReply<QVariantMap> reply(
          waitReply(f.session.asyncCall(call, 3000)));
      return reply.value();
    };
    QVERIFY(!state().value("protected").toBool());
    auto call = QDBusMessage::createMethodCall(
        "org.qindaqt.Lock1", "/org/qindaqt/Lock1", "org.qindaqt.Lock1",
        "RequestLock");
    auto pending = f.session.asyncCall(call, 3000);
    QTRY_COMPARE(f.backend.calls.size(), 1);
    f.backend.reply(true);
    const QDBusPendingReply<bool> admitted(waitReply(pending));
    QVERIFY(!admitted.isError());
    QVERIFY(admitted.value());
    QVERIFY(!state().value("protected").toBool());
    f.peer.process.write("state locking\n");
    QTRY_COMPARE(monitor.state(), LockState::Locking);
    QVERIFY(!state().value("protected").toBool());
    f.peer.process.write("state protected\n");
    QTRY_VERIFY(monitor.presentationProtected());
    QVERIFY(state().value("protected").toBool());
    QVERIFY(f.peer.unregisterService(QString(CompositorNames::service)));
    QVERIFY(!state().value("protected").toBool());
    QCOMPARE(state().value("state").toString(), "unknown");
  }
  void timeoutIsUncertainWithoutReplay() {
    Fixture f;
    QVERIFY(f.ready);
    QVERIFY(f.attach());
    QSignalSpy result(&f.request, &NativeLockRequest::completed);
    QVERIFY(f.request.request());
    QTRY_COMPARE(f.backend.calls.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 4000);
    QCOMPARE(result.first().first().value<RequestResult>(),
             RequestResult::Uncertain);
    QCOMPARE(f.backend.calls.size(), 1);
    f.backend.reply(true);
    QTest::qWait(10);
    QCOMPARE(result.size(), 1);
  }
};
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const auto args = app.arguments();
  if (args.size() == 4 && args[1] == "--backend")
    return backendMain(args[2], args[3]);
  RequestTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "tst_qt_native_lock_request.moc"
