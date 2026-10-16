// SPDX-License-Identifier: GPL-3.0-or-later
// Private-bus integration of the real supervisor lock assembly (ADR-0379):
// ScreenSaver.GetActive keeps answering through a frozen session bus and
// recovers after genuine compositor loss without a new login. Reproduces the
// 2026-10-15 failure, where one slow daemon reply left the lock Unknown.
#include "src/session_supervisor/src/native_lock_composition.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QtTest>
#include <csignal>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using namespace QindaQt;
namespace {
// Empty activation namespace: never spawn installed Settings1/Power1 here.
class PrivateDaemon final {
public:
  PrivateDaemon() {
    QFile config(directory.filePath(QStringLiteral("bus.conf")));
    if (!config.open(QIODevice::WriteOnly)) return;
    config.write("<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen>"
        "<auth>EXTERNAL</auth><policy context=\"default\"><allow send_destination=\"*\"/>"
        "<allow receive_sender=\"*\"/><allow own=\"*\"/></policy></busconfig>");
    config.close();
    process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
        {QStringLiteral("--config-file=") + config.fileName(), QStringLiteral("--nofork"),
         QStringLiteral("--print-address=1")});
    if (process.waitForStarted() && process.waitForReadyRead())
      address = QString::fromUtf8(process.readLine()).trimmed();
  }
  ~PrivateDaemon() { thaw(); process.terminate(); process.waitForFinished(5000); }
  void freeze() { ::kill(static_cast<pid_t>(process.processId()), SIGSTOP); }
  void thaw() { ::kill(static_cast<pid_t>(process.processId()), SIGCONT); }
  QTemporaryDir directory;
  QProcess process;
  QString address;
};
PrivateDaemon *sessionDaemon = nullptr;
QTemporaryDir *runtime = nullptr;
// Stand-in compositor: native lock receipts plus its Wayland socket.
class Compositor final : public QDBusVirtualObject {
public:
  explicit Compositor(QDBusConnection connection) : bus(std::move(connection)) {}
  QString introspect(const QString &) const override { return {}; }
  bool handleMessage(const QDBusMessage &m, const QDBusConnection &) override {
    if (m.interface() != QString(CompositorNames::nativeLockInterface)) return false;
    const bool request = m.member() == QStringLiteral("RequestLockWithReceipt");
    if (!request && m.member() != QStringLiteral("RequestStateWithReceipt")) return false;
    if (request) ++lockRequests;
    bus.send(m.createReply());
    auto receipt = QDBusMessage::createTargetedSignal(m.service(),
        QString(CompositorNames::nativeLockPath), QString(CompositorNames::nativeLockInterface),
        request ? QStringLiteral("lockAdmissionReceipt") : QStringLiteral("stateReceipt"));
    receipt << m.arguments().first().toString();
    if (request) receipt << true; else receipt << locked << protectedNow;
    bus.send(receipt);
    return true;
  }
  void state(bool lock, bool protection) {
    locked = lock; protectedNow = protection;
    auto m = QDBusMessage::createSignal(QString(CompositorNames::nativeLockPath),
        QString(CompositorNames::nativeLockInterface), QStringLiteral("lockedChanged"));
    m << locked; bus.send(m);
  }
  bool listen(const QString &path) {
    listener = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    const auto name = QFile::encodeName(path);
    std::memcpy(address.sun_path, name.constData(), static_cast<size_t>(name.size()) + 1);
    if (listener < 0 || ::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        ::listen(listener, 16) != 0) return false;
    // Match the compositor's socket: no group/other write access.
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                    QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                    QFileDevice::ExeOther);
    acceptor = std::make_unique<QSocketNotifier>(listener, QSocketNotifier::Read);
    QObject::connect(acceptor.get(), &QSocketNotifier::activated, acceptor.get(), [this] {
      int fd; while ((fd = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC)) >= 0) peers.append(fd);
    });
    return true;
  }
  ~Compositor() override {
    acceptor.reset();
    for (int fd : std::as_const(peers)) ::close(fd);
    if (listener >= 0) ::close(listener);
  }
  QDBusConnection bus;
  bool locked = false, protectedNow = false;
  int lockRequests = 0, listener = -1;
  QList<int> peers;
  std::unique_ptr<QSocketNotifier> acceptor;
};
QString getActive(const QDBusConnection &client) {
  // Asynchronous: the supervisor answers from this same thread.
  auto pending = client.asyncCall(QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"),
      QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("GetActive")), 3000);
  QElapsedTimer timer; timer.start();
  while (!pending.isFinished() && timer.elapsed() < 4000) QTest::qWait(1);
  const auto reply = pending.reply();
  if (reply.type() == QDBusMessage::ReplyMessage && reply.signature() == QStringLiteral("b"))
    return reply.arguments().first().toBool() ? QStringLiteral("active") : QStringLiteral("unlocked");
  return reply.errorName();
}
}
class NativeLockRecoveryTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void getActiveSurvivesStallAndRecoversWithoutNewLogin() {
    auto supervisor = QDBusConnection::sessionBus();
    QVERIFY(supervisor.isConnected());
    QVERIFY(supervisor.registerService(QStringLiteral("org.qindaqt.Session1")));
    auto compositorBus = QDBusConnection::connectToBus(sessionDaemon->address, QStringLiteral("compositor"));
    auto client = QDBusConnection::connectToBus(sessionDaemon->address, QStringLiteral("client"));
    Compositor compositor(compositorBus);
    QVERIFY(compositor.listen(runtime->filePath(QStringLiteral("qindaqt-1"))));
    QVERIFY(compositorBus.registerVirtualObject(QString(CompositorNames::nativeLockPath), &compositor));
    QVERIFY(compositorBus.registerService(QString(CompositorNames::service)));
    SessionSupervisor::NativeLockComposition composition;
    QString error;
    QVERIFY2(composition.start(&error, false), qPrintable(error));
    QTRY_COMPARE(getActive(client), QStringLiteral("unlocked"));

    // A frozen daemon for longer than the superseded 250 ms poll bound, while
    // the supervisor's event loop keeps running.
    sessionDaemon->freeze();
    QTest::qWait(700);
    sessionDaemon->thaw();
    QCOMPARE(getActive(client), QStringLiteral("unlocked"));
    compositor.state(true, true);
    QTRY_COMPARE(getActive(client), QStringLiteral("active"));
    compositor.state(false, false);
    QTRY_COMPARE(getActive(client), QStringLiteral("unlocked"));

    // Genuine loss publishes Unknown; the returning owner is re-proven and
    // the lock answers again without restarting the assembly.
    QVERIFY(compositorBus.unregisterService(QString(CompositorNames::service)));
    QTRY_COMPARE(getActive(client), QStringLiteral("org.qindaqt.Lock1.Error.Unavailable"));
    QVERIFY(compositorBus.registerService(QString(CompositorNames::service)));
    QTRY_COMPARE(getActive(client), QStringLiteral("unlocked"));

    // Manual lock works again on the re-attached display.
    auto pending = client.asyncCall(QDBusMessage::createMethodCall(
        QStringLiteral("org.qindaqt.Lock1"), QStringLiteral("/org/qindaqt/Lock1"),
        QStringLiteral("org.qindaqt.Lock1"), QStringLiteral("RequestLock")), 3000);
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().type(), QDBusMessage::ReplyMessage);
    QVERIFY(pending.reply().arguments().first().toBool());
    QCOMPARE(compositor.lockRequests, 1);
    composition.stop();
  }
};
int main(int argc, char **argv) {
  // AGENT-GUARD: route sessionBus() to the private daemon before anything
  // touches it, and give the supervisor no system bus (no logind).
  PrivateDaemon privateDaemon;
  QTemporaryDir privateRuntime;
  if (privateDaemon.address.isEmpty() || !privateRuntime.isValid()) return 2;
  sessionDaemon = &privateDaemon;
  runtime = &privateRuntime;
  qputenv("DBUS_SESSION_BUS_ADDRESS", privateDaemon.address.toUtf8());
  qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent");
  qputenv("XDG_RUNTIME_DIR", QFile::encodeName(privateRuntime.path()));
  qputenv("WAYLAND_DISPLAY", "qindaqt-1");
  QCoreApplication application(argc, argv);
  NativeLockRecoveryTests tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QDBusConnection::disconnectFromBus(QStringLiteral("compositor"));
  QDBusConnection::disconnectFromBus(QStringLiteral("client"));
  return result;
}
#include "tst_native_lock_recovery.moc"
