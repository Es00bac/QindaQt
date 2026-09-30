// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <functional>
#include <memory>
#include <optional>
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
// Same-thread identity/lifetime binding to one explicitly admitted ordinary
// display. This is not executable/supervisor attestation or locker authority.
// Admission selects an accepted session unique owner and is rechecked on every
// getter/open. Admission is readonly/non-reentrant: it must not query or mutate
// this attachment. Borrowed callback captures must outlive this QObject. No
// fallback. Signals may reenter public methods; callbacks may not.
class CompositorAttachment final : public QObject {
  Q_OBJECT
public:
  using SessionAdmission = std::function<bool(const QString &sessionOwner)>;
  CompositorAttachment(QDBusConnection bus, QString privateRuntimeDirectory,
                       SessionAdmission admission, QObject *parent = nullptr);
  ~CompositorAttachment() override;
  // Replaces the prior attachment, revoking it first. Optional independently
  // accepted expected peer is enforced in addition to actual socket/bus proof.
  bool attach(const QString &sessionOwner, const QString &socketBasename,
              std::optional<PeerExpectation> expectedPeer = std::nullopt);
  bool live() const;
  std::optional<AttachmentIdentity> identity() const;
  // Transfers one CLOEXEC connected ordinary FD (caller closes), or -1.
  // Caller must consume this descriptor rather than reconnecting by pathname.
  int openConnection();
  void revoke();
Q_SIGNALS:
  void attached();
  void revoked();

private:
  class Private;
  std::unique_ptr<Private> d;
};
} // namespace QindaQt::Platform::Compositor
