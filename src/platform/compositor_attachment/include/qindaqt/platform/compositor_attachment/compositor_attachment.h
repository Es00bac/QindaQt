// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <functional>
#include <memory>
#include <optional>
#include <qindaqt/platform/recovery_backoff/recovery_backoff.h>
namespace QindaQt::Platform::Compositor {
struct PeerExpectation final {
  QString uniqueOwner;
  quint64 pid = 0;
};
struct AttachmentIdentity final {
  QString sessionOwner;
  QString compositorOwner;
  quint64 compositorPid = 0;
  QString socketBasename;
};
// Re-attachment schedule and daemon-call bound. Production uses the defaults;
// deterministic tests inject manual timers.
struct AttachmentRecovery final {
  Recovery::BackoffPolicy policy{};
  Recovery::RecoveryTimerFactory timers{};
  int callTimeoutMilliseconds = 1000;
};
// Same-thread identity/lifetime binding to one explicitly admitted ordinary
// display. This is not executable/supervisor attestation or locker authority.
// Admission selects an accepted session unique owner and is rechecked on every
// getter/open; it must be cheap, readonly and non-reentrant (no blocking bus
// call). Borrowed callback captures must outlive this QObject. No fallback.
// Signals may reenter public methods; callbacks may not.
//
// AGENT-CONTRACT (ADR-0379, superseding ADR-0305's synchronous recheck):
// liveness is event-driven. Getters check the kernel PIDFD/probe socket and
// admission synchronously and use bus ownership cached from NameOwnerChanged;
// they never make a bus call. No timer polls the daemon. Genuine loss (peer
// exit, probe hang-up, compositor owner change, pathname replacement) emits
// revoked(); the retained selection then re-proves itself with bounded
// backoff and emits attached() again when a valid compositor owner appears.
// Slow or failed daemon replies are retried, never treated as loss. The
// selection is retired (no re-attachment) on revoke(), on a new attach(), when
// the selected session owner disconnects, or when admission refuses it.
// Admission is re-evaluated, and a refusal published as revoked(), whenever the
// selected session owner gives up a well-known name (for example Session1);
// any other withdrawal is visible to getters but published only by the
// consumer's own event (call revoke()).
class CompositorAttachment final : public QObject {
  Q_OBJECT
public:
  using SessionAdmission = std::function<bool(const QString &sessionOwner)>;
  CompositorAttachment(QDBusConnection bus, QString privateRuntimeDirectory,
                       SessionAdmission admission, QObject *parent = nullptr);
  CompositorAttachment(QDBusConnection bus, QString privateRuntimeDirectory,
                       SessionAdmission admission, AttachmentRecovery recovery,
                       QObject *parent = nullptr);
  ~CompositorAttachment() override;
  // Replaces the prior selection, revoking it first. Blocking proof with an
  // explicit per-call bound. An independently accepted expected peer is
  // enforced in addition to actual socket/bus proof, including on every
  // automatic re-attachment. False retains nothing.
  bool attach(const QString &sessionOwner, const QString &socketBasename,
              std::optional<PeerExpectation> expectedPeer = std::nullopt);
  bool live() const;
  // A retained selection is waiting to re-attach after genuine loss.
  bool recovering() const;
  // A live attachment and connection must name the same actual bus daemon.
  // The attachment's own connection is answered without a bus call; another
  // connection is compared by daemon GetId with the explicit call bound.
  bool sameBus(const QDBusConnection &connection) const;
  std::optional<AttachmentIdentity> identity() const;
  // Transfers one CLOEXEC connected ordinary FD (caller closes), or -1.
  // Caller must consume this descriptor rather than reconnecting by pathname.
  int openConnection();
  // Detaches and forgets the selection; no automatic re-attachment follows.
  void revoke();
Q_SIGNALS:
  void attached();
  void revoked();

private:
  class Private;
  std::unique_ptr<Private> d;
};
} // namespace QindaQt::Platform::Compositor
