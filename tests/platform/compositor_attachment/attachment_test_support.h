// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/platform/compositor_attachment/compositor_attachment.h"
#include <QDBusConnectionInterface>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
namespace AttachmentTestSupport {
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
// Freezes the private bus daemon to model a slow/stalled session bus. Every
// blocking daemon call made while frozen waits for its explicit bound.
class FrozenDaemon final {
public:
  explicit FrozenDaemon(Bus &bus) : pid(bus.daemon.processId()) {
    frozen = pid > 0 && ::kill(static_cast<pid_t>(pid), SIGSTOP) == 0;
  }
  ~FrozenDaemon() { thaw(); }
  void thaw() {
    if (frozen)
      ::kill(static_cast<pid_t>(pid), SIGCONT);
    frozen = false;
  }
  qint64 pid = 0;
  bool frozen = false;
};
inline QString socketName(int index = 0) {
  return QString(QindaQt::CompositorNames::waylandSocketPrefix) +
         QString::number(index);
}
} // namespace AttachmentTestSupport
