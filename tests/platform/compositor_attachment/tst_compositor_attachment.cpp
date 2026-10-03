// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/platform/compositor_attachment/compositor_attachment.h"
#include <QDBusConnectionInterface>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace QindaQt::Platform::Compositor;
class Bus final {
public:
  inline static int next = 0;
  int id = ++next;
  Bus() {
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {"--session", "--nofork", "--print-address=1"});
    if (!daemon.waitForStarted() || !daemon.waitForReadyRead())
      return;
    address = QString::fromUtf8(daemon.readLine()).trimmed();
  }
  ~Bus() {
    for (const auto &name : names)
      QDBusConnection::disconnectFromBus(name);
    daemon.terminate();
    daemon.waitForFinished(5000);
  }
  QDBusConnection connect(const QString &suffix) {
    const auto name = QStringLiteral("attachment-%1-%2-%3")
                          .arg(QCoreApplication::applicationPid())
                          .arg(id)
                          .arg(suffix);
    names.append(name);
    return QDBusConnection::connectToBus(address, name);
  }
  QProcess daemon;
  QString address;
  QStringList names;
};
class Listener final {
public:
  explicit Listener(QString value) : path(std::move(value)) {
    fd = create(path);
  }
  ~Listener() {
    if (fd >= 0)
      close(fd);
    QFile::remove(path);
  }
  static int create(const QString &path) {
    const int socketFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socketFd < 0)
      return -1;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto bytes = QFile::encodeName(path);
    if (bytes.size() >= qsizetype(sizeof(address.sun_path))) {
      close(socketFd);
      return -1;
    }
    std::memcpy(address.sun_path, bytes.constData(), size_t(bytes.size()) + 1);
    if (bind(socketFd, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0 ||
        listen(socketFd, 16) != 0) {
      close(socketFd);
      return -1;
    }
    // Match the 0755 socket created by the production QindaQt compositor.
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                    QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                    QFileDevice::ExeOther);
    return socketFd;
  }
  QString path;
  int fd = -1;
};
class OtherProcessListener final {
public:
  explicit OtherProcessListener(const QString &path) {
    int ready[2], done[2];
    if (pipe2(ready, O_CLOEXEC) != 0)
      return;
    if (pipe2(done, O_CLOEXEC) != 0) {
      close(ready[0]);
      close(ready[1]);
      return;
    }
    const auto bytes = QFile::encodeName(path);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (bytes.size() >= qsizetype(sizeof(address.sun_path))) {
      close(ready[0]);
      close(ready[1]);
      close(done[0]);
      close(done[1]);
      return;
    }
    std::memcpy(address.sun_path, bytes.constData(), size_t(bytes.size()) + 1);
    child = fork();
    if (child == 0) {
      close(ready[0]);
      close(done[1]);
      // Only async-signal-safe syscalls after fork from a Qt process.
      const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
      char result = fd >= 0 &&
                            bind(fd, reinterpret_cast<sockaddr *>(&address),
                                 sizeof(address)) == 0 &&
                            listen(fd, 16) == 0 &&
                            chmod(bytes.constData(), 0600) == 0
                        ? 1
                        : 0;
      {
        const auto ioResult = write(ready[1], &result, 1);
        (void)ioResult;
      }
      close(ready[1]);
      char finish = 0;
      {
        const auto ioResult = read(done[0], &finish, 1);
        (void)ioResult;
      }
      if (fd >= 0)
        close(fd);
      close(done[0]);
      _exit(0);
    }
    close(ready[1]);
    close(done[0]);
    if (child < 0) {
      close(ready[0]);
      close(done[1]);
      return;
    }
    char result = 0;
    available = read(ready[0], &result, 1) == 1 && result == 1;
    close(ready[0]);
    completion = done[1];
  }
  ~OtherProcessListener() {
    if (completion >= 0) {
      const char value = 1;
      {
        const auto ioResult = write(completion, &value, 1);
        (void)ioResult;
      }
      close(completion);
    }
    if (child > 0) {
      int status = 0;
      (void)waitpid(child, &status, 0);
    }
  }
  pid_t child = -1;
  int completion = -1;
  bool available = false;
};
class AttachmentTest final : public QObject {
  Q_OBJECT
private slots:
  void pinsActualOrdinaryPeerAndTransfersCloexecDescriptor();
  void initialAdvertisementDoesNotRevokeButActualLossDoes();
  void separateDaemonCannotReuseOwnerAndPid();
  void rejectsDeniedSelectionAndIncorrectIndependentPeer();
  void authorityLossRevokesBeforeQueuedWatchers();
  void rejectsUnsafePathsAndCanonicalNameViolations();
  void sameUidWrongProcessAndPathReplacementCannotAttach();
  void reentrantRevocationCannotReportSuccessfulAttachment();
  void kernelPeerDeathRevokesWithoutWaitingForOwnerWatcher();
  void selectedSessionOwnerLossRevokesIdentity();
};
void AttachmentTest::pinsActualOrdinaryPeerAndTransfersCloexecDescriptor() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(compositor.isConnected() && session.isConnected());
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  Listener socket(runtime.filePath(
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0"));
  QVERIFY(socket.fd >= 0);
  struct stat socketInfo{};
  QVERIFY(::stat(QFile::encodeName(socket.path).constData(), &socketInfo) == 0);
  QCOMPARE(socketInfo.st_mode & 0777, mode_t(0755));
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QVERIFY(attachment.attach(
      session.baseService(), QFileInfo(socket.path).fileName(),
      PeerExpectation{compositor.baseService(), quint64(getpid())}));
  QVERIFY(attachment.live());
  QVERIFY(attachment.identity());
  QCOMPARE(attachment.identity()->compositorOwner, compositor.baseService());
  QCOMPARE(attachment.identity()->compositorPid, quint64(getpid()));
  const int transferred = attachment.openConnection();
  QVERIFY(transferred >= 3);
  QVERIFY((fcntl(transferred, F_GETFD) & FD_CLOEXEC) != 0);
  close(transferred);
  QSignalSpy revoked(&attachment, &CompositorAttachment::revoked);
  attachment.revoke();
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
  QCOMPARE(revoked.count(), 1);
}
void AttachmentTest::initialAdvertisementDoesNotRevokeButActualLossDoes() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session"), client = fixture.connect("client");
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  QVERIFY(socket.fd >= 0);
  CompositorAttachment attachment(
      client, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QTest::qWait(10);
  QSignalSpy revoked(&attachment, &CompositorAttachment::revoked);
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(attachment.attach(session.baseService(), name));
  QTest::qWait(150);
  QVERIFY(attachment.live());
  QVERIFY(revoked.isEmpty());
  QVERIFY(
      compositor.unregisterService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTRY_COMPARE(revoked.size(), 1);
  QVERIFY(!attachment.live());
}
void AttachmentTest::separateDaemonCannotReuseOwnerAndPid() {
  Bus first, second;
  auto compositor = first.connect("compositor"),
       session = first.connect("session"), client = first.connect("client");
  auto other = second.connect("compositor"),
       otherSession = second.connect("session"),
       otherClient = second.connect("client");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(other.registerService(QString(QindaQt::CompositorNames::service)));
  QCOMPARE(compositor.baseService(), other.baseService());
  QCOMPARE(client.interface()->servicePid(compositor.baseService()).value(),
           otherClient.interface()->servicePid(other.baseService()).value());
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  QVERIFY(socket.fd >= 0);
  CompositorAttachment attachment(
      client, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QVERIFY(!attachment.sameBus(client));
  QVERIFY(attachment.attach(session.baseService(), name));
  QVERIFY(attachment.sameBus(client));
  QVERIFY(attachment.sameBus(session));
  QVERIFY(!attachment.sameBus(otherClient));
}
void AttachmentTest::rejectsDeniedSelectionAndIncorrectIndependentPeer() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session"), other = fixture.connect("other");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  QVERIFY(socket.fd >= 0);
  CompositorAttachment denied(compositor, runtime.path(),
                              [](const QString &) { return false; });
  QVERIFY(!denied.attach(session.baseService(), name));
  QVERIFY(!denied.identity());
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QVERIFY(!attachment.attach(other.baseService(), name));
  QVERIFY(!attachment.attach(
      session.baseService(), name,
      PeerExpectation{other.baseService(), quint64(getpid())}));
  QVERIFY(!attachment.attach(
      session.baseService(), name,
      PeerExpectation{compositor.baseService(), quint64(getpid()) + 1}));
  QVERIFY(attachment.attach(session.baseService(), name));
}
void AttachmentTest::authorityLossRevokesBeforeQueuedWatchers() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  bool admitted = true;
  CompositorAttachment attachment(
      compositor, runtime.path(), [&](const QString &owner) {
        return admitted && owner == session.baseService();
      });
  QVERIFY(attachment.attach(session.baseService(), name));
  admitted = false;
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
  admitted = true;
  QVERIFY(attachment.attach(session.baseService(), name));
  QVERIFY(
      compositor.unregisterService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
}
void AttachmentTest::rejectsUnsafePathsAndCanonicalNameViolations() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  for (const auto &invalid :
       {"../qindaqt-0", "qindaqt-00", "qindaqt-4096", "qindaqt-", "wayland-0"})
    QVERIFY(!attachment.attach(session.baseService(), invalid));
  const auto alias = runtime.filePath(
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "1");
  QVERIFY(QFile::link(socket.path, alias));
  QVERIFY(
      !attachment.attach(session.baseService(), QFileInfo(alias).fileName()));
  QVERIFY(::chmod(QFile::encodeName(socket.path).constData(), 0777) == 0);
  QVERIFY(!attachment.attach(session.baseService(), name));
  QVERIFY(::chmod(QFile::encodeName(socket.path).constData(), 0755) == 0);
  QVERIFY(QFile::setPermissions(
      runtime.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                          QFileDevice::ExeOwner | QFileDevice::ReadGroup));
  QVERIFY(!attachment.attach(session.baseService(), name));
  QVERIFY(QFile::setPermissions(runtime.path(), QFileDevice::ReadOwner |
                                                    QFileDevice::WriteOwner |
                                                    QFileDevice::ExeOwner));
  QVERIFY(attachment.attach(session.baseService(), name));
}
void AttachmentTest::sameUidWrongProcessAndPathReplacementCannotAttach() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  const auto path = runtime.filePath(name);
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  {
    OtherProcessListener wrong(path);
    QVERIFY(wrong.available);
    QVERIFY(!attachment.attach(session.baseService(), name));
  }
  QVERIFY(QFile::remove(path));
  Listener legitimate(path);
  QVERIFY(legitimate.fd >= 0);
  QVERIFY(attachment.attach(session.baseService(), name));
  QVERIFY(QFile::remove(path));
  OtherProcessListener replaced(path);
  QVERIFY(replaced.available);
  QCOMPARE(attachment.openConnection(), -1);
  QVERIFY(!attachment.identity());
}
void AttachmentTest::reentrantRevocationCannotReportSuccessfulAttachment() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  connect(&attachment, &CompositorAttachment::attached, &attachment,
          &CompositorAttachment::revoke);
  QVERIFY(!attachment.attach(session.baseService(), name));
  QVERIFY(!attachment.identity());
}
void AttachmentTest::selectedSessionOwnerLossRevokesIdentity() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  const auto selected = session.baseService();
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == selected; });
  QVERIFY(attachment.attach(selected, name));
  QDBusConnection::disconnectFromBus(session.name());
  session = QDBusConnection(QStringLiteral("disconnected-fixture"));
  // Confirm daemon-observed loss synchronously; do not pump the object's
  // queued owner watcher before checking the public getter.
  const auto registered = compositor.interface()->isServiceRegistered(selected);
  QVERIFY(registered.isValid());
  QVERIFY(!registered.value());
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
}

void AttachmentTest::kernelPeerDeathRevokesWithoutWaitingForOwnerWatcher() {
  Bus fixture;
  auto client = fixture.connect("client"), session = fixture.connect("session");
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  QProcess peer;
  peer.start(QCoreApplication::applicationFilePath(),
             {"--peer", fixture.address, runtime.filePath(name)});
  QVERIFY(peer.waitForStarted());
  QVERIFY(peer.waitForReadyRead());
  const auto owner = QString::fromUtf8(peer.readLine()).trimmed();
  QVERIFY(owner.startsWith(':'));
  CompositorAttachment attachment(client, runtime.path(),
                                  [&](const QString &selected) {
                                    return selected == session.baseService();
                                  });
  QVERIFY(attachment.attach(session.baseService(), name,
                            PeerExpectation{owner, quint64(peer.processId())}));
  QVERIFY(attachment.live());
  peer.terminate();
  QVERIFY(peer.waitForFinished(5000));
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  if (app.arguments().size() == 4 &&
      app.arguments().at(1) == QStringLiteral("--peer")) {
    auto bus = QDBusConnection::connectToBus(app.arguments().at(2), "peer");
    if (!bus.isConnected() ||
        !bus.registerService(QString(QindaQt::CompositorNames::service)))
      return 1;
    Listener socket(app.arguments().at(3));
    if (socket.fd < 0)
      return 2;
    const auto owner = bus.baseService().toUtf8();
    std::puts(owner.constData());
    std::fflush(stdout);
    return app.exec();
  }
  AttachmentTest tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "tst_compositor_attachment.moc"
