// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/file_manager_types.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

namespace QindaQt::Apps::FileManager {

class RemoteCopier;

// Owns the remote-copy-to state machine so NavigationController does not
// accumulate it (ADR-0155), mirroring RemoteRenameController: at most one
// copy in flight, fenced by the listing generation captured at dispatch.
// NavigationController supplies the current listing/URL/generation for
// each request and surfaces refresh/failure; this collaborator validates
// source and destination, dispatches, fences results, and retires the job
// on cancellation or destruction (the copier owns the KIO job lifetime).
class RemoteCopyToController : public QObject {
  Q_OBJECT

public:
  explicit RemoteCopyToController(RemoteCopier &copier, QObject *parent = nullptr);

  [[nodiscard]] bool busy() const noexcept { return m_busy; }

  // Validates sourcePath (one immediate listed child of remoteUrl) and the
  // destination folder text, then dispatches one copy when everything
  // checks out. Emits failure() and returns false on any refusal; emits
  // busyChanged() and returns true when dispatched. A confirmed success
  // emits refreshRequested() only when the destination is the folder
  // remoteUrl names -- copying elsewhere changes nothing visible here.
  bool requestCopy(const QVector<DirectoryEntry> &listedEntries, const QUrl &remoteUrl,
                   quint64 listingGeneration, const QString &sourcePath,
                   const QString &destinationFolder);

  // Retires an in-flight copy (replacement navigation, leaving remote).
  // The cancelled job's late result is fenced out by the generation.
  void cancelPending();

signals:
  void busyChanged();
  // Confirmed success whose destination is the current folder: the caller
  // re-reads the authoritative remote listing.
  void refreshRequested();
  // A refusal (pre-dispatch validation) or a typed failure/cancellation
  // result that passed the generation fence.
  void failure(const QString &message);

private:
  void onCopyFinished(quint64 generation, const QString &diagnostic);

  RemoteCopier *m_copier;
  bool m_busy = false;
  quint64 m_generation = 0;
  // Destination captured at dispatch: only a success landing here changes
  // the visible folder.
  QUrl m_destinationFolder;
  // Active folder captured at dispatch for the refresh decision.
  QUrl m_currentFolder;
};

} // namespace QindaQt::Apps::FileManager
