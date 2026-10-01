// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../services/native_lock_service/private_bus.h"
#include "../../apps/settings/screensaver/screensaver_model_test_support.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/native_lock_service/qt_native_lock_request.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <qindaqt/session/native_sleep/sleep_service.h>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusUnixFileDescriptor>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
namespace SleepTest {
using namespace QindaQt;
using namespace Session::NativeSleep;
using namespace Services::SessionLockState;
using namespace Platform::Compositor;
struct UserWire { quint32 uid; QDBusObjectPath path; };
inline QDBusArgument &operator<<(QDBusArgument &a, const UserWire &u) {
  a.beginStructure(); a << u.uid << u.path; a.endStructure(); return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, UserWire &u) {
  a.beginStructure(); a >> u.uid >> u.path; a.endStructure(); return a;
}
}
Q_DECLARE_METATYPE(SleepTest::UserWire)
namespace SleepTest {
class FakeLogind final : public QDBusVirtualObject {
public:
  explicit FakeLogind(QDBusConnection connection) : bus(std::move(connection)) { qDBusRegisterMetaType<UserWire>(); }
  ~FakeLogind() override { for (int fd : readEnds) ::close(fd); }
  QString introspect(const QString &) const override { return {}; }
  bool handleMessage(const QDBusMessage &m, const QDBusConnection &) override {
    if (m.member() == "GetSession") {
      selectedIds.append(m.arguments().first().toString());
      bus.send(m.createReply(QVariant::fromValue(QDBusObjectPath(path))));
    } else if (m.member() == "GetSessionByPID") {
      selectedPids.append(m.arguments().first().toUInt());
      bus.send(m.createReply(QVariant::fromValue(QDBusObjectPath(pidPath))));
    } else if (m.member() == "GetAll") {
      QVariantMap properties{{"Id", id}, {"User", QVariant::fromValue(UserWire{
          uid, QDBusObjectPath(QStringLiteral("/org/freedesktop/login1/user/_%1").arg(uid))})}};
      if (malformedUser) properties["User"] = true;
      bus.send(m.createReply(QVariant::fromValue(properties)));
    } else if (m.member() == "Inhibit") {
      inhibitors.append(m.arguments());
      if (deferInhibit) deferredInhibit = m;
      else sendInhibit(m);
    } else if (m.member() == "SetLockedHint") {
      hints.append(m.arguments().first().toBool());
      bus.send(m.createReply());
    } else if (m.member() == "CanSuspend") {
      ++canCalls;
      if (deferCan) deferredCan = m;
      else bus.send(m.createReply(canAnswer));
    } else if (m.member() == "Suspend") {
      ++suspendCalls;
      interactive = m.arguments().first().toBool();
      if (autoPrepare) prepare(true);
      bus.send(m.createReply());
    } else return false;
    return true;
  }
  void sendInhibit(const QDBusMessage &m) {
    int descriptors[2]; QVERIFY(::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) == 0);
    readEnds.append(descriptors[0]);
    QDBusUnixFileDescriptor fd(descriptors[1]); ::close(descriptors[1]);
    bus.send(m.createReply(QVariant::fromValue(fd)));
  }
  bool inhibitorClosed(qsizetype index = 0) {
    char byte; return ::read(readEnds.at(index), &byte, 1) == 0;
  }
  void signal(const QString &member, const QString &signalPath, QVariantList args = {},
              QDBusConnection *sender = nullptr) {
    auto m = QDBusMessage::createSignal(signalPath,
        signalPath == QStringLiteral("/org/freedesktop/login1")
            ? QStringLiteral("org.freedesktop.login1.Manager")
            : QStringLiteral("org.freedesktop.login1.Session"), member);
    m.setArguments(args); (sender ? *sender : bus).send(m);
  }
  void prepare(bool value, QDBusConnection *sender = nullptr) {
    signal(QStringLiteral("PrepareForSleep"), QStringLiteral("/org/freedesktop/login1"), {value}, sender);
  }
  void claim() {
    QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/login1"), this,
                                      QDBusConnection::SubPath));
    QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.login1")));
  }
  QDBusConnection bus;
  QString id = QStringLiteral("native1");
  QString path = QStringLiteral("/org/freedesktop/login1/session/_1");
  QString pidPath = path;
  quint32 uid = static_cast<quint32>(getuid());
  bool malformedUser = false, deferInhibit = false, deferCan = false, autoPrepare = false;
  bool interactive = true;
  QString canAnswer = QStringLiteral("yes");
  int canCalls = 0, suspendCalls = 0;
  QStringList selectedIds;
  QList<quint32> selectedPids;
  QList<QVariantList> inhibitors;
  QList<int> readEnds;
  QList<bool> hints;
  QDBusMessage deferredInhibit, deferredCan;
};
class NativeWire final : public QDBusVirtualObject {
public:
  explicit NativeWire(QDBusConnection connection) : bus(std::move(connection)) {}
  QString introspect(const QString &) const override { return {}; }
  bool handleMessage(const QDBusMessage &m, const QDBusConnection &) override {
    if (m.interface() != QString(CompositorNames::nativeLockInterface)) return false;
    const bool request = m.member() == "RequestLockWithReceipt";
    if (!request && m.member() != "RequestStateWithReceipt") return false;
    if (request) ++requests;
    bus.send(m.createReply());
    if (omitReceipt) return true;
    auto receipt = QDBusMessage::createTargetedSignal(m.service(),
        QString(CompositorNames::nativeLockPath), QString(CompositorNames::nativeLockInterface),
        request ? QStringLiteral("lockAdmissionReceipt") : QStringLiteral("stateReceipt"));
    receipt << (wrongNonce ? QString(32, u'0') : m.arguments().first().toString());
    if (request) receipt << admit; else receipt << locked << protectedNow;
    bus.send(receipt);
    return true;
  }
  void state(bool lock, bool protection) {
    locked = lock; protectedNow = protection;
    auto m = QDBusMessage::createSignal(QString(CompositorNames::nativeLockPath),
        QString(CompositorNames::nativeLockInterface), QStringLiteral("lockedChanged"));
    m << locked; bus.send(m);
  }
  QDBusConnection bus;
  bool locked = false, protectedNow = false, admit = true, wrongNonce = false, omitReceipt = false;
  int requests = 0;
};
class NoIdle final : public Platform::Idle::IdleObservation {
public:
  void setTimeout(int) override {}
  bool available() const override { return false; }
  bool idle() const override { return false; }
  void refresh() override {}
  void revoke() override {}
};
struct Fixture {
  PrivateBus broker;
  QDBusConnection supervisor = broker.connect("supervisor");
  QDBusConnection logindBus = broker.connect("logind");
  QDBusConnection compositorBus = broker.connect("compositor");
  QDBusConnection attacker = broker.connect("attacker");
  FakeLogind logind{logindBus};
  NativeWire native{compositorBus};
  QTemporaryDir directory;
  int listener = -1;
  QList<int> peers;
  std::unique_ptr<QSocketNotifier> acceptor;
  bool supervisorAdmitted = true;
  CompositorAttachment attachment{supervisor, directory.path(), [this](const QString &owner) {
    return supervisorAdmitted && owner == supervisor.baseService();
  }};
  QtNativeLockTransport nativeTransport{supervisor};
  NativeLockStateMonitor monitor{nativeTransport, [this](const QString &owner, quint64 pid) {
    const auto identity = attachment.identity();
    return identity && attachment.sameBus(supervisor) &&
        identity->compositorOwner == owner && identity->compositorPid == pid;
  }};
  Services::NativeLock::QtNativeLockRequest request{supervisor, attachment};
  ScreensaverTestSupport::FakeSettingsTransport settingsTransport;
  Services::SettingsClient::SettingsClient settings{settingsTransport, Services::LockPreferences::scopedKeys()};
  Services::LockPreferences::PreferencesProvider preferences{settings};
  NoIdle idle;
  Session::NativeLockRuntime::Runtime runtime{preferences, idle, request, monitor, nullptr};
  LogindSleepTransport transport{supervisor, "native1", static_cast<quint32>(getuid()),
      static_cast<quint32>(getpid()), [this] { return attachment.live(); }, static_cast<quint32>(getuid())};
  SleepCoordinator coordinator{transport, runtime, monitor};
  SleepService service{supervisor, coordinator, static_cast<quint32>(getuid())};
  void startNative() {
    QVERIFY(supervisor.registerService(QStringLiteral("org.qindaqt.Session1")));
    QVERIFY(compositorBus.registerService(QString(CompositorNames::service)));
    QVERIFY(compositorBus.registerVirtualObject(QString(CompositorNames::nativeLockPath), &native));
    listener = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    QVERIFY(listener >= 0);
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    const auto name = QFile::encodeName(directory.filePath("qindaqt-1"));
    std::memcpy(address.sun_path, name.constData(), static_cast<size_t>(name.size()) + 1);
    QVERIFY(::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
    QVERIFY(::listen(listener, 16) == 0);
    QFile::setPermissions(directory.filePath("qindaqt-1"), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    acceptor = std::make_unique<QSocketNotifier>(listener, QSocketNotifier::Read);
    QObject::connect(acceptor.get(), &QSocketNotifier::activated, acceptor.get(), [this] {
      int fd; while ((fd = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC)) >= 0) peers.append(fd);
    });
    QVERIFY(attachment.attach(supervisor.baseService(), QStringLiteral("qindaqt-1"),
        PeerExpectation{compositorBus.baseService(), static_cast<quint64>(getpid())}));
    QVERIFY(monitor.start());
    settingsTransport.setValue("lock.automaticEnabled", false);
    settingsTransport.setValue("lock.onResume", true);
    settingsTransport.setValue("lock.idleTimeoutSeconds", QVariant::fromValue<qint64>(300));
    settingsTransport.setValue("lock.graceSeconds", QVariant::fromValue<qint64>(0));
    QVERIFY(settings.start()); settingsTransport.announceOwner();
    QTRY_VERIFY(settings.snapshot().has_value());
    QVERIFY(runtime.start());
    QTRY_COMPARE(monitor.state(), LockState::Unlocked);
  }
  void start() {
    startNative(); logind.claim(); coordinator.start(); QVERIFY(service.start());
    QTRY_VERIFY(transport.hasDelayInhibitor());
  }
  ~Fixture() {
    service.stop(); coordinator.stop(); runtime.stop(); monitor.stop();
    acceptor.reset();
    for (int fd : peers) ::close(fd);
    if (listener >= 0) ::close(listener);
  }
};
}
