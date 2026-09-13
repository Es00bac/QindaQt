// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/file_manager_types.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

namespace QindaQt::Apps::FileManager {

class RemoteMover;

// Owns the remote-move-to state machine so NavigationController does not
// accumulate it (ADR-0156), mirroring RemoteCopyToController: at most one
// move in flight, fenced by the listing generation captured at dispatch.
// A move is destructive -- the server may delete the source even on a
// mid-move failure -- so every policy check runs here before dispatch and
// the result is never anticipated in the visible listing.
// NavigationController supplies the current listing/URL/generation for each
// request and surfaces refresh/failure; this collaborator validates source
// and destination, dispatches, fences results, and retires the job on
// cancellation or destruction (the mover owns the KIO job lifetime).
class RemoteMoveToController : public QObject {
  Q_OBJECT

public:
  explicit RemoteMoveToController(RemoteMover &mover, QObject *parent = nullptr);

  [[nodiscard]] bool busy() const noexcept { return m_busy; }

  // Validates sourcePath (one immediate listed child of remoteUrl) and the
  // destination folder text, then dispatches one move when everything
  // checks out. Emits failure() and returns false on any refusal; emits
  // busyChanged() and returns true when dispatched. A confirmed success
  // always emits refreshRequested(): the moved source was a listed child of
  // the folder remoteUrl names, so the visible folder always lost an entry.
  bool requestMove(const QVector<DirectoryEntry> &listedEntries, const QUrl &remoteUrl,
                   quint64 listingGeneration, const QString &sourcePath,
                   const QString &destinationFolder);

  // Retires an in-flight move (replacement navigation, leaving remote).
  // The cancelled job's late result is fenced out by the generation.
  void cancelPending();

signals:
  void busyChanged();
  // Confirmed success: the caller re-reads the authoritative remote listing
  // -- a move always removes its source from the folder being viewed, and
  // nothing changed optimistically before this signal.
  void refreshRequested();
  // A refusal (pre-dispatch validation) or a typed failure/cancellation
  // result that passed the generation fence.
  void failure(const QString &message);

private:
  void onMoveFinished(quint64 generation, const QString &diagnostic);

  RemoteMover *m_mover;
  bool m_busy = false;
  quint64 m_generation = 0;
};

} // namespace QindaQt::Apps::FileManager
