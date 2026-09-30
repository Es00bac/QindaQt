// SPDX-License-Identifier: GPL-3.0-or-later
#include "attachment_socket_p.h"
#include <QFile>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
namespace QindaQt::Platform::Compositor::Private {
struct Descriptor {
  int value = -1;
  explicit Descriptor(int fd = -1) : value(fd) {}
  Descriptor(const Descriptor &) = delete;
  Descriptor &operator=(const Descriptor &) = delete;
  ~Descriptor() {
    if (value >= 0)
      close(value);
  }
  int release() {
    const int fd = value;
    value = -1;
    return fd;
  }
};
bool nativeName(const QString &name) {
  const auto prefix = QString(QindaQt::CompositorNames::waylandSocketPrefix);
  if (!name.startsWith(prefix))
    return false;
  const auto suffix = name.mid(prefix.size());
  if (suffix.isEmpty() || suffix.size() > 4)
    return false;
  for (const auto c : suffix)
    if (c < '0' || c > '9')
      return false;
  bool ok = false;
  const auto index = suffix.toUInt(&ok);
  return ok && index <= 4095 && QString::number(index) == suffix;
}
int privateDirectory(const QString &path) {
  if (!path.startsWith('/') || path.contains("//"))
    return -1;
  Descriptor current(open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  for (const auto &part : path.mid(1).split('/')) {
    if (part.isEmpty() || part == "." || part == "..")
      return -1;
    Descriptor next(openat(current.value, QFile::encodeName(part).constData(),
                           O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (next.value < 0)
      return -1;
    close(current.value);
    current.value = next.release();
  }
  struct stat value{};
  if (fstat(current.value, &value) != 0 || value.st_uid != geteuid() ||
      (value.st_mode & 07777) != 0700)
    return -1;
  return current.release();
}
int connectPeer(const QString &runtime, const QString &name, qint64 expected,
                int *pidfd) {
  Descriptor directory(privateDirectory(runtime));
  if (directory.value < 0)
    return -1;
  struct stat entry{};
  const auto file = QFile::encodeName(name);
  if (fstatat(directory.value, file.constData(), &entry, AT_SYMLINK_NOFOLLOW) !=
          0 ||
      !S_ISSOCK(entry.st_mode) || entry.st_uid != geteuid() ||
      entry.st_nlink != 1 || (entry.st_mode & 0007) != 0)
    return -1;
  Descriptor socketFd(
      socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
  if (socketFd.value < 0)
    return -1;
  if (socketFd.value < 3) {
    const int duplicate = fcntl(socketFd.value, F_DUPFD_CLOEXEC, 3);
    if (duplicate < 0)
      return -1;
    close(socketFd.value);
    socketFd.value = duplicate;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  const auto endpoint = QByteArray("/proc/self/fd/") +
                        QByteArray::number(directory.value) + '/' + file;
  if (endpoint.size() >= static_cast<qsizetype>(sizeof(address.sun_path)))
    return -1;
  std::memcpy(address.sun_path, endpoint.constData(),
              static_cast<std::size_t>(endpoint.size()) + 1);
  if (::connect(socketFd.value, reinterpret_cast<sockaddr *>(&address),
                sizeof(address)) != 0)
    return -1;
  ucred peer{};
  socklen_t size = sizeof(peer);
  if (getsockopt(socketFd.value, SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0 ||
      peer.uid != geteuid() || peer.pid != expected)
    return -1;
  int held = -1;
  socklen_t heldSize = sizeof(held);
  if (getsockopt(socketFd.value, SOL_SOCKET, SO_PEERPIDFD, &held, &heldSize) !=
      0)
    return -1;
  Descriptor life(held);
  pollfd status{held, POLLIN, 0};
  if (heldSize != sizeof(held) || poll(&status, 1, 0) != 0)
    return -1;
  if (pidfd)
    *pidfd = life.release();
  return socketFd.release();
}
} // namespace QindaQt::Platform::Compositor::Private
