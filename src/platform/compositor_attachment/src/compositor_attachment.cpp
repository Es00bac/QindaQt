// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/platform/compositor_attachment/compositor_attachment.h"
#include "attachment_proof_p.h"
#include "attachment_socket_p.h"
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <algorithm>
#include <cerrno>
#include <poll.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <sys/socket.h>
#include <unistd.h>
namespace QindaQt::Platform::Compositor {
Q_LOGGING_CATEGORY(lcAttachment, "qindaqt.compositor-attachment", QtInfoMsg)
namespace {
bool uniqueName(const QString &name) {
  static const QRegularExpression pattern(QStringLiteral("^:[0-9]+\\.[0-9]+$"));
  return name.size() <= 255 && pattern.match(name).hasMatch();
}
// QtDBus delivers an argument-matched signal only to a QObject slot.
class NameReleaseRelay final : public QObject {
  Q_OBJECT
public:
  explicit NameReleaseRelay(std::function<void()> released)
      : m_released(std::move(released)) {}
public Q_SLOTS:
  void nameOwnerChanged(const QString &, const QString &, const QString &) {
    m_released();
  }

private:
  std::function<void()> m_released;
};
} // namespace
namespace detail = ::QindaQt::Platform::Compositor::Private;
using detail::PeerStatus;
class CompositorAttachment::Private {
public:
  struct Selection {
    QString sessionOwner, basename;
    std::optional<PeerExpectation> expected;
  };
  Private(CompositorAttachment &owner, QDBusConnection connection, QString path,
          SessionAdmission admit, const AttachmentRecovery &recovery)
      : q(owner), bus(std::move(connection)), runtime(std::move(path)),
        admission(std::move(admit)),
        callTimeout(std::max(1, recovery.callTimeoutMilliseconds)),
        watcher(QString(QindaQt::CompositorNames::service), bus,
                QDBusServiceWatcher::WatchForOwnerChange, &q),
        released([this] { sessionOwnerReleasedName(); }),
        backoff(recovery.policy, recovery.timers, &q) {
    QObject::connect(&watcher, &QDBusServiceWatcher::serviceOwnerChanged, &q,
                     [this](const QString &name, const QString &oldOwner,
                            const QString &newOwner) {
                       ownerChanged(name, oldOwner, newOwner);
                     });
    QObject::connect(&backoff, &Recovery::RecoveryBackoff::retry, &q,
                     [this] { tryReattach(); });
  }
  ~Private() {
    cancelProof();
    unwatchReleasedNames();
    closeCurrent();
  }
  bool admitted(const QString &sessionOwner) const {
    return admission && admission(sessionOwner);
  }
  bool kernelLive() const {
    if (probe < 0 || pidfd < 0)
      return false;
    pollfd life{pidfd, POLLIN, 0},
        socket{probe, static_cast<short>(POLLIN | POLLRDHUP), 0};
    return poll(&life, 1, 0) == 0 && poll(&socket, 1, 0) >= 0 &&
           (socket.revents & (POLLHUP | POLLERR | POLLNVAL | POLLRDHUP)) == 0;
  }
  bool live() const {
    return current && kernelLive() && admitted(current->sessionOwner);
  }
  void closeCurrent() {
    // AGENT-GUARD: disable notifiers before closing their FDs, and delete them
    // later: loss is usually detected inside their own activation.
    for (auto *notifier : {lifeNotifier.release(), probeNotifier.release()}) {
      if (!notifier)
        continue;
      notifier->setEnabled(false);
      notifier->disconnect(&q);
      notifier->deleteLater();
    }
    if (probe >= 0)
      close(probe);
    if (pidfd >= 0)
      close(pidfd);
    probe = pidfd = -1;
    current.reset();
  }
  void forgetSelection() {
    if (selection)
      watcher.removeWatchedService(selection->sessionOwner);
    unwatchReleasedNames();
    selection.reset();
    cancelProof();
    backoff.reset();
    loggedFailure = false;
  }
  // Publishes revoked() when an attachment was bound. A retained selection
  // schedules re-attachment unless a reentrant observer replaced it.
  void detach(const QString &reason, bool retain) {
    const bool bound = bool(current);
    const auto serial = ++generation;
    closeCurrent();
    if (!retain)
      forgetSelection();
    if (bound) {
      qCInfo(lcAttachment).noquote()
          << "compositor attachment detached:" << reason
          << (retain && selection ? "(will re-attach)" : "");
      Q_EMIT q.revoked();
    }
    if (serial == generation && retain && selection && !current)
      scheduleRecovery();
  }
  void retire(const QString &reason) {
    if (!current && selection)
      qCInfo(lcAttachment).noquote()
          << "compositor attachment selection retired:" << reason;
    detach(reason, false);
  }
  void scheduleRecovery() {
    if (selection && !current)
      backoff.schedule();
  }
  void cancelProof() {
    ++proofSerial;
    if (proof) {
      proof->cancel();
      proof.release()->deleteLater();
    }
  }
  void tryReattach() {
    if (!selection || current)
      return;
    if (!bus.isConnected()) {
      retire(QStringLiteral("session bus connection closed"));
      return;
    }
    if (!admitted(selection->sessionOwner)) {
      retire(QStringLiteral("session admission withdrawn"));
      return;
    }
    cancelProof();
    const auto serial = proofSerial;
    proof = std::make_unique<detail::AsyncProof>(
        bus, selection->sessionOwner, callTimeout,
        [this, serial](const detail::ProofResult &result) {
          proofFinished(serial, result);
        });
    proof->start();
  }
  void proofFinished(quint64 serial, const detail::ProofResult &result) {
    if (serial != proofSerial || !selection || current)
      return;
    if (result.outcome == detail::ProofOutcome::Retire) {
      retire(result.reason);
      return;
    }
    QString reason = result.reason;
    if (result.outcome == detail::ProofOutcome::Proven) {
      const auto &facts = result.facts;
      const auto &expected = selection->expected;
      if (expected && (expected->uniqueOwner != facts.compositorOwner ||
                       expected->pid != facts.compositorPid)) {
        retire(QStringLiteral("independently expected compositor is gone"));
        return;
      }
      auto peer = detail::connectPeer(runtime, selection->basename,
                                      static_cast<qint64>(facts.compositorPid),
                                      true);
      if (peer.status == PeerStatus::Connected) {
        if (!admitted(selection->sessionOwner)) {
          close(peer.fd);
          close(peer.pidfd);
          retire(QStringLiteral("session admission withdrawn"));
          return;
        }
        install(facts, peer);
        backoff.reset();
        loggedFailure = false;
        qCInfo(lcAttachment).noquote()
            << "compositor attachment re-attached to" << facts.compositorOwner
            << "pid" << facts.compositorPid;
        Q_EMIT q.attached();
        return;
      }
      reason = peer.status == PeerStatus::Mismatch
                   ? QStringLiteral("display pathname does not reach the "
                                    "proven compositor")
                   : QStringLiteral("display socket is not accepting yet");
    }
    if (!loggedFailure) {
      loggedFailure = true;
      qCInfo(lcAttachment).noquote()
          << "compositor re-attachment deferred:" << reason;
    }
    scheduleRecovery();
  }
  void install(const detail::ProofFacts &facts,
               const detail::PeerConnection &peer) {
    current = std::make_unique<AttachmentIdentity>(
        AttachmentIdentity{selection->sessionOwner, facts.compositorOwner,
                           facts.compositorPid, selection->basename});
    probe = peer.fd;
    pidfd = peer.pidfd;
    lifeNotifier =
        std::make_unique<QSocketNotifier>(pidfd, QSocketNotifier::Read);
    QObject::connect(lifeNotifier.get(), &QSocketNotifier::activated, &q,
                     [this] { kernelLoss(); });
    probeNotifier =
        std::make_unique<QSocketNotifier>(probe, QSocketNotifier::Read);
    QObject::connect(probeNotifier.get(), &QSocketNotifier::activated, &q,
                     [this] { probeReadable(); });
  }
  // An independently expected peer cannot come back once its process
  // exited, so its selection retires instead of waiting forever.
  void kernelLoss() {
    const bool expectedGone = current && selection && selection->expected &&
                              selection->expected->pid == current->compositorPid;
    detach(QStringLiteral("compositor process exited"), !expectedGone);
  }
  void probeReadable() {
    char byte = 0;
    const auto count = ::recv(probe, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    if (count > 0) {
      // The compositor never writes to an idle probe; PIDFD keeps lifetime.
      if (probeNotifier)
        probeNotifier->setEnabled(false);
      return;
    }
    if (count < 0 &&
        (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
      return;
    detach(QStringLiteral("compositor closed the display probe"), true);
  }
  // AGENT-NOTE (ADR-0379): consumers admit "the current Session1 owner" or a
  // similar authority. Withdrawal is published by event, not polled: a match on
  // NameOwnerChanged arg1 (old owner) == the selected owner re-checks
  // admission whenever that owner gives up any well-known name.
  bool releaseMatch(const QString &owner, bool add) {
    const auto daemon = QStringLiteral("org.freedesktop.DBus");
    const QStringList match{QString(), owner}; // null arg0: any name
    const char *slot = SLOT(nameOwnerChanged(QString,QString,QString));
    return add ? bus.connect(daemon, QStringLiteral("/org/freedesktop/DBus"),
                             daemon, QStringLiteral("NameOwnerChanged"), match,
                             QString(), &released, slot)
               : bus.disconnect(daemon, QStringLiteral("/org/freedesktop/DBus"),
                                daemon, QStringLiteral("NameOwnerChanged"),
                                match, QString(), &released, slot);
  }
  void watchReleasedNames(const QString &owner) {
    unwatchReleasedNames();
    if (releaseMatch(owner, true))
      releaseWatched = owner;
  }
  void unwatchReleasedNames() {
    if (!releaseWatched.isEmpty())
      releaseMatch(releaseWatched, false);
    releaseWatched.clear();
  }
  void sessionOwnerReleasedName() {
    // Disconnection of the owner itself also matches; the owner watcher
    // retires that case and a second retire is a no-op.
    if (selection && !admitted(selection->sessionOwner))
      retire(QStringLiteral("session admission withdrawn"));
  }
  void ownerChanged(const QString &name, const QString &oldOwner,
                    const QString &newOwner) {
    if (selection && name == selection->sessionOwner) {
      if (newOwner.isEmpty())
        retire(QStringLiteral("selected session owner disconnected"));
      return;
    }
    if (name != QString(QindaQt::CompositorNames::service))
      return;
    // Only the admitted owner's own loss detaches: queued advertisements and
    // stale events about earlier owners are not changes. Actual loss detaches
    // even when the same owner later reclaims the name; the reclaim then
    // re-proves the selection like any other new owner.
    if (current && oldOwner == current->compositorOwner &&
        newOwner != current->compositorOwner)
      detach(newOwner.isEmpty()
                 ? QStringLiteral("compositor released its bus name")
                 : QStringLiteral("compositor bus name moved to %1")
                       .arg(newOwner),
             true);
    if (!current && selection && !newOwner.isEmpty()) {
      backoff.cancel();
      tryReattach();
    }
  }
  CompositorAttachment &q;
  QDBusConnection bus;
  QString runtime;
  SessionAdmission admission;
  int callTimeout;
  QDBusServiceWatcher watcher;
  NameReleaseRelay released;
  QString releaseWatched;
  Recovery::RecoveryBackoff backoff;
  std::optional<Selection> selection;
  std::unique_ptr<AttachmentIdentity> current;
  std::unique_ptr<QSocketNotifier> lifeNotifier, probeNotifier;
  std::unique_ptr<detail::AsyncProof> proof;
  int probe = -1, pidfd = -1;
  quint64 generation = 0, proofSerial = 0;
  bool loggedFailure = false;
};
CompositorAttachment::CompositorAttachment(QDBusConnection bus, QString runtime,
                                           SessionAdmission admission,
                                           QObject *parent)
    : CompositorAttachment(std::move(bus), std::move(runtime),
                           std::move(admission), AttachmentRecovery{}, parent) {}
CompositorAttachment::CompositorAttachment(QDBusConnection bus, QString runtime,
                                           SessionAdmission admission,
                                           AttachmentRecovery recovery,
                                           QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(*this, std::move(bus), std::move(runtime),
                                  std::move(admission), recovery)) {}
CompositorAttachment::~CompositorAttachment() = default;
bool CompositorAttachment::attach(const QString &sessionOwner,
                                  const QString &basename,
                                  std::optional<PeerExpectation> expected) {
  d->detach(QStringLiteral("replaced by a new selection"), false);
  if (!detail::nativeName(basename) || !uniqueName(sessionOwner) ||
      !d->bus.isConnected() || !d->admitted(sessionOwner))
    return false;
  // AGENT-GUARD: watch the selected owner before proving it registered, so a
  // disconnect between the proof and the watch cannot go unnoticed.
  d->watcher.addWatchedService(sessionOwner);
  d->watchReleasedNames(sessionOwner);
  const auto refuse = [this, &sessionOwner] {
    d->watcher.removeWatchedService(sessionOwner);
    d->unwatchReleasedNames();
    return false;
  };
  const auto proof =
      detail::proveSynchronously(d->bus, sessionOwner, d->callTimeout);
  if (proof.outcome != detail::ProofOutcome::Proven)
    return refuse();
  const auto &facts = proof.facts;
  if (expected && (expected->uniqueOwner != facts.compositorOwner ||
                   expected->pid != facts.compositorPid))
    return refuse();
  const auto peer = detail::connectPeer(
      d->runtime, basename, static_cast<qint64>(facts.compositorPid), true);
  if (peer.status != PeerStatus::Connected)
    return refuse();
  d->selection = Private::Selection{sessionOwner, basename, expected};
  d->install(facts, peer);
  if (!d->live()) {
    d->detach(QStringLiteral("attachment failed its final liveness check"),
              false);
    return false;
  }
  const auto serial = d->generation;
  qCInfo(lcAttachment).noquote()
      << "compositor attachment bound to" << facts.compositorOwner << "pid"
      << facts.compositorPid << "socket" << basename;
  Q_EMIT attached();
  return serial == d->generation && d->live();
}
bool CompositorAttachment::live() const { return d->live(); }
bool CompositorAttachment::recovering() const {
  return d->selection.has_value() && !d->current;
}
bool CompositorAttachment::sameBus(const QDBusConnection &connection) const {
  if (!d->live() || !connection.isConnected())
    return false;
  // The same libdbus connection necessarily talks to the same daemon.
  if (connection.internalPointer() == d->bus.internalPointer())
    return true;
  const auto request = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetId"));
  const QDBusReply<QString> own =
      d->bus.call(request, QDBus::Block, d->callTimeout);
  const QDBusReply<QString> peer =
      connection.call(request, QDBus::Block, d->callTimeout);
  return own.isValid() && peer.isValid() && !own.value().isEmpty() &&
         own.value() == peer.value() && d->live();
}
std::optional<AttachmentIdentity> CompositorAttachment::identity() const {
  return d->live() ? std::optional<AttachmentIdentity>(*d->current)
                   : std::nullopt;
}
int CompositorAttachment::openConnection() {
  if (!d->live()) {
    if (d->current && !d->kernelLive())
      d->kernelLoss();
    return -1;
  }
  const auto peer = detail::connectPeer(
      d->runtime, d->current->socketBasename,
      static_cast<qint64>(d->current->compositorPid), false);
  if (peer.status == PeerStatus::Mismatch) {
    d->detach(QStringLiteral("display pathname no longer reaches the attached "
                             "compositor"),
              true);
    return -1;
  }
  if (peer.status != PeerStatus::Connected)
    return -1;
  if (!d->live()) {
    close(peer.fd);
    return -1;
  }
  // AGENT-GUARD: transfer this validated ordinary FD. Reconnecting by basename
  // permits pathname replacement after identity/lifetime admission (ADR-0305).
  return peer.fd;
}
void CompositorAttachment::revoke() {
  d->detach(QStringLiteral("revoked by its owner"), false);
}
} // namespace QindaQt::Platform::Compositor
#include "compositor_attachment.moc"
