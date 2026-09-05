// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/workspaces/workspace_transport.h"

#include <QDBusConnection>
#include <QList>
#include <QPointer>
#include <QString>

#include <optional>

class QDBusMessage;
class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::Shell::Workspaces {

// Who may be believed as the compositor. The supervisor-provisioned PID is
// the primary fact; when the shell has none, the PID of the exact owner of
// `peerServiceName` (QindaQt's own compositor plugin service, which lives in
// the KWin process) is the fallback. With neither, no owner is ever bound.
struct WorkspaceAuthority {
  std::optional<qint64> compositorProcessId;
  QString peerServiceName = QStringLiteral("org.qindaqt.Compositor");
};

// Narrow adapter over KWin's public `org.kde.KWin` and
// `org.kde.KWin.VirtualDesktopManager` D-Bus surface (ADR-0075).
//
// AGENT-CONTRACT: the adapter binds the exact unique owner of `org.kde.KWin`
// only after `GetConnectionUnixProcessID` for that owner equals the injected
// compositor PID (or the peer service owner's PID). Every method call and
// signal subscription then targets that unique owner, never the well-known
// name, so a replaced compositor cannot answer a request made to its
// predecessor. Owner changes publish an empty owner first and re-resolve.
//
// It reads `count`, `current`, `rows`, `desktops` and `showingDesktop`; it
// writes only the `current` property and calls only `showDesktop(bool)`.
// Desktop creation, removal, and renaming are deliberately not exposed.
class QtWorkspaceTransport final : public WorkspaceTransport {
  Q_OBJECT
public:
  static constexpr int RequestTimeoutMilliseconds = 5'000;

  QtWorkspaceTransport(QDBusConnection connection, WorkspaceAuthority authority,
                       QObject *parent = nullptr);
  ~QtWorkspaceTransport() override;

  [[nodiscard]] bool start(QString *error = nullptr) override;
  void stop() override;
  void requestSnapshot(quint64 token, const QString &uniqueOwner) override;
  void requestSwitch(quint64 token, const QString &uniqueOwner,
                     const QString &desktopId) override;
  void requestShowDesktop(quint64 token, const QString &uniqueOwner,
                          bool showing) override;

  [[nodiscard]] const QString &boundOwner() const noexcept { return m_owner; }

private Q_SLOTS:
  void handleWorkspaceSignal();
  void handleServiceOwnerChanged(const QString &service, const QString &oldOwner,
                                 const QString &newOwner);

private:
  void resolveOwner();
  void authenticate(quint64 generation, const QString &kwinOwner,
                    qint64 kwinProcessId, qint64 expectedProcessId);
  void bindOwner(const QString &owner);
  void unbindOwner(const QString &reasonCode);
  void requestNameOwner(quint64 generation, const QString &service,
                        void (QtWorkspaceTransport::*next)(quint64, const QString &,
                                                           const QString &),
                        const QString &context);
  void requestProcessId(quint64 generation, const QString &uniqueOwner,
                        void (QtWorkspaceTransport::*next)(quint64, const QString &,
                                                           qint64),
                        const QString &context);
  void handleKWinOwner(quint64 generation, const QString &context,
                       const QString &owner);
  void handleKWinProcessId(quint64 generation, const QString &context,
                           qint64 processId);
  void handlePeerOwner(quint64 generation, const QString &context,
                       const QString &owner);
  void handlePeerProcessId(quint64 generation, const QString &context,
                           qint64 processId);
  void finishSnapshot(quint64 token, const QString &owner,
                      WorkspaceSnapshot snapshot);
  QDBusPendingCallWatcher *track(const QDBusMessage &message);
  void untrack(QDBusPendingCallWatcher *watcher);

  QDBusConnection m_connection;
  WorkspaceAuthority m_authority;
  QDBusServiceWatcher *m_serviceWatcher = nullptr;
  QList<QPointer<QDBusPendingCallWatcher>> m_pendingCalls;
  QString m_owner;
  QString m_pendingKWinOwner;
  qint64 m_pendingKWinProcessId = 0;
  quint64 m_resolveGeneration = 0;
  bool m_started = false;
  bool m_signalsBound = false;
};

} // namespace QindaQt::Shell::Workspaces
