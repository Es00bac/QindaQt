// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/file_manager_types.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

namespace QindaQt::Apps::FileManager {

class RemoteRenamer;

// Owns the remote-rename state machine so NavigationController does not
// accumulate it (ADR-0153): at most one rename in flight, fenced by the
// listing generation captured at dispatch. NavigationController supplies
// the current listing/URL/generation snapshot for each request and decides
// what refresh/failure mean for the visible folder; this collaborator
// validates, dispatches, fences results, and retires the job on
// cancellation or destruction (the renamer owns the KIO job lifetime).
class RemoteRenameController : public QObject {
  Q_OBJECT

public:
  explicit RemoteRenameController(RemoteRenamer &renamer, QObject *parent = nullptr);

  [[nodiscard]] bool busy() const noexcept { return m_busy; }

  // Validates sourcePath/newName against the listed children of remoteUrl
  // and dispatches one same-folder rename when everything checks out.
  // Emits failure() and returns false on any refusal; emits busyChanged()
  // and returns true when dispatched.
  bool requestRename(const QVector<DirectoryEntry> &listedEntries, const QUrl &remoteUrl,
                     quint64 listingGeneration, const QString &sourcePath,
                     const QString &newName);

  // Retires an in-flight rename (replacement navigation, leaving remote).
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
  void onRenameFinished(quint64 generation, const QString &diagnostic);

  RemoteRenamer *m_renamer;
  bool m_busy = false;
  quint64 m_generation = 0;
};

} // namespace QindaQt::Apps::FileManager
