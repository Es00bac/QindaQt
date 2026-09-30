// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/platform/compositor_attachment/compositor_attachment.h"
#include "attachment_socket_p.h"
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QRegularExpression>
#include <QTimer>
#include <poll.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <unistd.h>
namespace QindaQt::Platform::Compositor {
namespace {
bool uniqueName(const QString &name) {
  static const QRegularExpression pattern(QStringLiteral("^:[0-9]+\\.[0-9]+$"));
  return name.size() <= 255 && pattern.match(name).hasMatch();
}
} // namespace
class CompositorAttachment::Private {
public:
  Private(CompositorAttachment &owner, QDBusConnection connection, QString path,
          SessionAdmission admit)
      : q(owner), bus(std::move(connection)), runtime(std::move(path)),
        admission(std::move(admit)),
        watcher(QString(QindaQt::CompositorNames::service), bus,
                QDBusServiceWatcher::WatchForOwnerChange, &q) {
    if (bus.interface())
      bus.interface()->setTimeout(250);
    QObject::connect(
        &watcher, &QDBusServiceWatcher::serviceOwnerChanged, &q,
        [this](const QString &, const QString &oldOwner, const QString &) {
          // A queued initial advertisement may arrive after attach
          // already admitted that exact owner. Actual loss of the
          // retained owner revokes even if it later reclaims the name.
          if (current && (oldOwner == current->compositorOwner || !live()))
            clear();
        });
    check.setInterval(100);
    QObject::connect(&check, &QTimer::timeout, &q, [this] {
      if (!live())
        clear();
    });
  }
  ~Private() { clear(false); }
  bool sameOwners() const {
    if (!current || !bus.isConnected() || !bus.interface() || !admission ||
        !admission(current->sessionOwner))
      return false;
    const auto owner = bus.interface()->serviceOwner(
        QString(QindaQt::CompositorNames::service));
    const auto pid = bus.interface()->servicePid(current->compositorOwner);
    const auto uid = bus.interface()->serviceUid(current->compositorOwner);
    const auto session =
        bus.interface()->isServiceRegistered(current->sessionOwner);
    const auto sessionUid = bus.interface()->serviceUid(current->sessionOwner);
    return owner.isValid() && owner.value() == current->compositorOwner &&
           pid.isValid() && pid.value() == current->compositorPid &&
           uid.isValid() && uid.value() == geteuid() && session.isValid() &&
           session.value() && sessionUid.isValid() &&
           sessionUid.value() == geteuid();
  }
  bool live() const {
    if (!current || probe < 0 || pidfd < 0)
      return false;
    pollfd life{pidfd, POLLIN, 0},
        socket{probe, static_cast<short>(POLLIN | POLLRDHUP), 0};
    return poll(&life, 1, 0) == 0 && poll(&socket, 1, 0) >= 0 &&
           (socket.revents & (POLLHUP | POLLERR | POLLNVAL | POLLRDHUP)) == 0 &&
           sameOwners();
  }
  void clear(bool publish = true) {
    ++generation;
    const bool bound = bool(current);
    if (current)
      watcher.removeWatchedService(current->sessionOwner);
    check.stop();
    if (probe >= 0)
      close(probe);
    if (pidfd >= 0)
      close(pidfd);
    probe = -1;
    pidfd = -1;
    current.reset();
    if (bound && publish)
      Q_EMIT q.revoked();
  }
  CompositorAttachment &q;
  QDBusConnection bus;
  QString runtime;
  SessionAdmission admission;
  QDBusServiceWatcher watcher;
  QTimer check;
  std::unique_ptr<AttachmentIdentity> current;
  int probe = -1, pidfd = -1;
  quint64 generation = 0;
};
CompositorAttachment::CompositorAttachment(QDBusConnection bus, QString runtime,
                                           SessionAdmission admission,
                                           QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(*this, std::move(bus), std::move(runtime),
                                  std::move(admission))) {}
CompositorAttachment::~CompositorAttachment() = default;
bool CompositorAttachment::attach(const QString &sessionOwner,
                                  const QString &basename,
                                  std::optional<PeerExpectation> expected) {
  d->clear();
  if (!::QindaQt::Platform::Compositor::Private::nativeName(basename) ||
      !uniqueName(sessionOwner) || !d->bus.isConnected() ||
      !d->bus.interface() || !d->admission || !d->admission(sessionOwner))
    return false;
  const auto owner = d->bus.interface()->serviceOwner(
      QString(QindaQt::CompositorNames::service));
  if (!owner.isValid() || !uniqueName(owner.value()))
    return false;
  const auto pid = d->bus.interface()->servicePid(owner.value());
  const auto uid = d->bus.interface()->serviceUid(owner.value());
  const auto session = d->bus.interface()->isServiceRegistered(sessionOwner);
  const auto sessionUid = d->bus.interface()->serviceUid(sessionOwner);
  if (!pid.isValid() || pid.value() <= 1 || !uid.isValid() ||
      uid.value() != geteuid() || !session.isValid() || !session.value() ||
      !sessionUid.isValid() || sessionUid.value() != geteuid())
    return false;
  if (expected &&
      (expected->uniqueOwner != owner.value() || expected->pid != pid.value()))
    return false;
  int life = -1;
  const int fd = ::QindaQt::Platform::Compositor::Private::connectPeer(
      d->runtime, basename, pid.value(), &life);
  if (fd < 0)
    return false;
  d->current = std::make_unique<AttachmentIdentity>(
      AttachmentIdentity{sessionOwner, owner.value(), pid.value(), basename});
  d->probe = fd;
  d->pidfd = life;
  d->watcher.addWatchedService(sessionOwner);
  if (!d->live()) {
    d->clear();
    return false;
  }
  d->check.start();
  const auto serial = d->generation;
  Q_EMIT attached();
  return serial == d->generation && d->live();
}
bool CompositorAttachment::live() const { return d->live(); }
bool CompositorAttachment::sameBus(const QDBusConnection &connection) const {
  if (!d->live() || !connection.isConnected())
    return false;
  const auto request = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetId"));
  const QDBusReply<QString> own = d->bus.call(request, QDBus::Block, 250);
  const QDBusReply<QString> peer = connection.call(request, QDBus::Block, 250);
  return own.isValid() && peer.isValid() && !own.value().isEmpty() &&
         own.value() == peer.value() && d->live();
}
std::optional<AttachmentIdentity> CompositorAttachment::identity() const {
  return d->live() ? std::optional<AttachmentIdentity>(*d->current)
                   : std::nullopt;
}
int CompositorAttachment::openConnection() {
  if (!d->live()) {
    d->clear();
    return -1;
  }
  const int fd = ::QindaQt::Platform::Compositor::Private::connectPeer(
      d->runtime, d->current->socketBasename,
      static_cast<qint64>(d->current->compositorPid));
  if (fd < 0 || !d->live()) {
    if (fd >= 0)
      close(fd);
    d->clear();
    return -1;
  }
  // AGENT-GUARD: transfer this validated ordinary FD. Reconnecting by basename
  // permits pathname replacement after identity/lifetime admission (ADR-0305).
  return fd;
}
void CompositorAttachment::revoke() { d->clear(); }
} // namespace QindaQt::Platform::Compositor
