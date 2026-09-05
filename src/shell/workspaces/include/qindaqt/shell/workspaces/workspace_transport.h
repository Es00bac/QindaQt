// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell/workspaces/workspace_types.h"

#include <QObject>
#include <QString>

namespace QindaQt::Shell::Workspaces {

// Injected compositor seam for workspace truth and intents. Production binds
// the authenticated compositor owner over Qt D-Bus; tests inject a fake.
//
// AGENT-CONTRACT (owner lineage): every request names the exact unique owner
// the caller observed. Implementations must answer with that same owner (or
// fail the request) so a reply from a replaced compositor can never be joined
// to the previous owner's snapshot. `ownerChanged` with an empty owner means
// "no authenticated compositor"; `reasonCode` then names why.
//
// AGENT-CONTRACT (threading): GUI-thread confined, no internal locking. The
// controller owns request tokens; implementations echo them unchanged.
class WorkspaceTransport : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~WorkspaceTransport() override = default;

  [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
  virtual void stop() = 0;
  virtual void requestSnapshot(quint64 token, const QString &uniqueOwner) = 0;
  virtual void requestSwitch(quint64 token, const QString &uniqueOwner,
                             const QString &desktopId) = 0;
  virtual void requestShowDesktop(quint64 token, const QString &uniqueOwner,
                                  bool showing) = 0;

Q_SIGNALS:
  void ownerChanged(const QString &uniqueOwner, const QString &reasonCode);
  // The bound owner announced a change; the controller re-reads a snapshot.
  void changed(const QString &uniqueOwner);
  void snapshotReceived(quint64 token, const QString &uniqueOwner,
                        const QindaQt::Shell::Workspaces::WorkspaceSnapshot &snapshot);
  void snapshotFailed(quint64 token, const QString &uniqueOwner,
                      const QString &reasonCode);
  void operationFinished(quint64 token, const QString &uniqueOwner, bool ok,
                         const QString &reasonCode);
};

} // namespace QindaQt::Shell::Workspaces
