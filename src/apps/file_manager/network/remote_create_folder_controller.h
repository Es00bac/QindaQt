// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace QindaQt::Apps::FileManager {

class RemoteFolderCreator;

// Owns the remote-folder-create state machine so NavigationController does
// not accumulate it (ADR-0154), mirroring RemoteRenameController: at most
// one create in flight, fenced by the listing generation captured at
// dispatch. NavigationController supplies the active remote folder and
// generation for each request and decides what refresh/failure mean for
// the visible folder; this collaborator validates the name, proves the
// target parent is the active folder, dispatches, fences results, and
// retires the job on cancellation or destruction (the creator owns the KIO
// job lifetime).
class RemoteCreateFolderController : public QObject {
  Q_OBJECT

public:
  explicit RemoteCreateFolderController(RemoteFolderCreator &creator, QObject *parent = nullptr);

  [[nodiscard]] bool busy() const noexcept { return m_busy; }

  // Validates name against the active remote folder and dispatches one
  // directory creation when everything checks out. Emits failure() and
  // returns false on any refusal; emits busyChanged() and returns true when
  // dispatched.
  bool requestCreate(const QUrl &remoteUrl, quint64 listingGeneration, const QString &name);

  // Retires an in-flight create (replacement navigation, leaving remote).
  // The cancelled job's late result is fenced out by the generation.
  void cancelPending();

signals:
  void busyChanged();
  // Success: the caller re-reads the authoritative remote listing.
  void refreshRequested();
  // A refusal (pre-dispatch validation) or a typed failure/cancellation
  // result that passed the generation fence.
  void failure(const QString &message);

private:
  void onCreateFinished(quint64 generation, const QString &diagnostic);

  RemoteFolderCreator *m_creator;
  bool m_busy = false;
  quint64 m_generation = 0;
};

} // namespace QindaQt::Apps::FileManager
