// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/workspaces/workspace_transport.h"

#include <QList>

namespace QindaQt::Tests::Workspaces {

// Recording seam for the workspace controller and the desktop-controls
// composition. The test drives every reply explicitly; nothing is answered
// automatically, so ordering and lineage bugs surface as missing state.
class FakeWorkspaceTransport final : public Shell::Workspaces::WorkspaceTransport {
public:
  using WorkspaceTransport::WorkspaceTransport;

  struct SnapshotRequest {
    quint64 token = 0;
    QString owner;
  };
  struct SwitchRequest {
    quint64 token = 0;
    QString owner;
    QString desktopId;
  };
  struct ShowDesktopRequest {
    quint64 token = 0;
    QString owner;
    bool showing = false;
  };

  bool start(QString *) override
  {
    ++startCalls;
    return true;
  }
  void stop() override { ++stopCalls; }
  void requestSnapshot(quint64 token, const QString &owner) override
  {
    snapshotRequests.append({token, owner});
  }
  void requestSwitch(quint64 token, const QString &owner,
                     const QString &desktopId) override
  {
    switchRequests.append({token, owner, desktopId});
  }
  void requestShowDesktop(quint64 token, const QString &owner, bool showing) override
  {
    showDesktopRequests.append({token, owner, showing});
  }

  void announce(const QString &owner, const QString &reason = {})
  {
    Q_EMIT ownerChanged(owner, reason);
  }
  void change(const QString &owner) { Q_EMIT changed(owner); }
  void reply(const SnapshotRequest &request,
             const Shell::Workspaces::WorkspaceSnapshot &snapshot)
  {
    Q_EMIT snapshotReceived(request.token, request.owner, snapshot);
  }
  void fail(const SnapshotRequest &request, const QString &reason)
  {
    Q_EMIT snapshotFailed(request.token, request.owner, reason);
  }
  void finishSwitch(const SwitchRequest &request, bool ok, const QString &reason = {})
  {
    Q_EMIT operationFinished(request.token, request.owner, ok, reason);
  }
  void finishShowDesktop(const ShowDesktopRequest &request, bool ok,
                         const QString &reason = {})
  {
    Q_EMIT operationFinished(request.token, request.owner, ok, reason);
  }

  QList<SnapshotRequest> snapshotRequests;
  QList<SwitchRequest> switchRequests;
  QList<ShowDesktopRequest> showDesktopRequests;
  int startCalls = 0;
  int stopCalls = 0;
};

inline Shell::Workspaces::WorkspaceSnapshot fixtureSnapshot(
    const QString &currentId = QStringLiteral("ws-2"), bool showingDesktop = false)
{
  Shell::Workspaces::WorkspaceSnapshot snapshot;
  snapshot.desktops = {{0, QStringLiteral("ws-1"), QStringLiteral("Main")},
                       {1, QStringLiteral("ws-2"), QStringLiteral("Code")},
                       {2, QStringLiteral("ws-3"), QString{}}};
  snapshot.currentId = currentId;
  snapshot.rows = 1;
  snapshot.showingDesktop = showingDesktop;
  return snapshot;
}

} // namespace QindaQt::Tests::Workspaces
