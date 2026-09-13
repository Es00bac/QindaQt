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
// move in flight, fenced by a monotonic, never-reused operation identity.
// A move is destructive -- the server may delete the source even on a
// mid-move failure -- so every policy check runs here before dispatch and
// the result is never anticipated in the visible listing.
// NavigationController supplies the current listing/URL/generation for each
// request and surfaces refresh/failure; this collaborator validates source
// and destination, dispatches, fences results, and retires the job on
// cancellation or destruction (the mover owns the KIO job lifetime).
//
// AGENT-CONTRACT (reviewed P1 repair): the listing generation is the
// source-freshness fence at dispatch -- the caller passes the generation of
// the listing snapshot the request is validated against, so an unchanged
// listing admits a retry while a superseded one cannot reach dispatch.
// Result correlation must NOT reuse that generation: Cancel followed by an
// immediate retry in the same listing would alias the cancelled job's late
// result onto the replacement (it retires B, exposes the stale error, or
// triggers B's refresh). Every accepted move therefore gets its own
// operation identity: a counter incremented per accepted move and never
// reused or reset -- not even by cancellation or completion.
class RemoteMoveToController : public QObject {
  Q_OBJECT

public:
  explicit RemoteMoveToController(RemoteMover &mover, QObject *parent = nullptr);

  [[nodiscard]] bool busy() const noexcept { return m_busy; }

  // Validates sourcePath (one immediate listed child of remoteUrl) and the
  // destination folder text, then dispatches one move when everything
  // checks out. Emits failure() and returns false on any refusal; emits
  // busyChanged() and returns true when dispatched. listingGeneration is
  // the source-freshness fence (see the class contract); the dispatched
  // move is identified by a fresh monotonic operation identity, not by it.
  // A confirmed success always emits refreshRequested(): the moved source
  // was a listed child of the folder remoteUrl names, so the visible folder
  // always lost an entry.
  bool requestMove(const QVector<DirectoryEntry> &listedEntries, const QUrl &remoteUrl,
                   quint64 listingGeneration, const QString &sourcePath,
                   const QString &destinationFolder);

  // Retires an in-flight move (replacement navigation, leaving remote).
  // The cancelled job's late result is fenced out by the operation identity.
  void cancelPending();

signals:
  void busyChanged();
  // Confirmed success: the caller re-reads the authoritative remote listing
  // -- a move always removes its source from the folder being viewed, and
  // nothing changed optimistically before this signal.
  void refreshRequested();
  // A refusal (pre-dispatch validation) or a typed failure/cancellation
  // result that passed the operation-identity fence.
  void failure(const QString &message);

private:
  void onMoveFinished(quint64 operation, const QString &diagnostic);

  RemoteMover *m_mover;
  bool m_busy = false;
  // Operation identity of the in-flight move (and the value handed to the
  // mover as the request identity): incremented for every accepted move
  // and NEVER reused or reset, so a cancelled move's late result can never
  // alias an immediate retry in the same unchanged listing.
  quint64 m_operation = 0;
};

} // namespace QindaQt::Apps::FileManager
