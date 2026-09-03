// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <qindaqt/services/clipboard_model/clipboard_types.h>
#include "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"

namespace QindaQt::ShellClipboardApplet {

// AGENT-CONTRACT: Injected least-authority public client seam for the Clipboard Applet.
// The applet controller depends strictly on this interface. It never links or executes
// raw Wayland protocol requests, never accesses host clipboards, and never directly
// mutates internal storage without generation fencing.
//
// Threading: the seam is GUI-thread confined. The controller invokes every
// virtual member directly on its own thread and expects every signal to be
// emitted on that same thread. A seam backed by another thread must marshal
// through Qt queued connections (the values crossing the boundary are
// implicitly-shared Qt copies, safe to hand over after the emitting call
// returns); re-entrant signal emission from inside a dispatch call is
// supported and fenced by the controller through request-id attribution.
//
// Lifetime: the controller borrows the seam and never deletes it. The
// composing shell must guarantee the client outlives the controller (or
// parents it accordingly); destroying the client first is undefined behavior
// for any in-flight controller call.
//
// Errors and results: the request members perform no synchronous error
// reporting — outcomes arrive only through operationCompleted/searchCompleted,
// each echoing the unique request id of exactly one request. Request ids are
// opaque, unique for the client's lifetime across all request kinds, and
// carry NO ordering promise (zero is a valid id like any other): consumers
// must fence replies by recorded id, never by id arithmetic. Completions must
// carry the entry lineage they resolve; the controller additionally rejects a
// completion whose entry id disagrees with the id recorded at dispatch.
// Direct virtual calls are pure reads (snapshot/state accessors) and never
// mutate anything.
class ClipboardClientInterface : public QObject {
    Q_OBJECT

public:
    explicit ClipboardClientInterface(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~ClipboardClientInterface() override = default;

    [[nodiscard]] virtual ClientState clientState() const noexcept = 0;
    [[nodiscard]] virtual QString reasonCode() const = 0;
    [[nodiscard]] virtual QString owner() const = 0;
    [[nodiscard]] virtual bool isOwnerAvailable() const noexcept = 0;
    [[nodiscard]] virtual bool isLocked() const noexcept = 0;
    [[nodiscard]] virtual QindaQt::Services::ClipboardModel::HistorySnapshot snapshot() const = 0;

    // Intent request dispatches returning unique request IDs.
    virtual quint64 requestPromote(QindaQt::Services::ClipboardModel::EntryId id,
                                   quint32 expectedGeneration,
                                   quint64 tick) = 0;
    virtual quint64 requestRemove(QindaQt::Services::ClipboardModel::EntryId id,
                                  quint32 expectedGeneration) = 0;
    virtual quint64 requestSetPinned(QindaQt::Services::ClipboardModel::EntryId id,
                                     bool pinned,
                                     quint32 expectedGeneration) = 0;
    virtual quint64 requestClear(QindaQt::Services::ClipboardModel::ClearScope scope,
                                 quint32 expectedGeneration) = 0;
    virtual quint64 requestSearch(const QString &query,
                                  quint32 expectedGeneration,
                                  int maxResults) = 0;

Q_SIGNALS:
    void stateChanged(QindaQt::ShellClipboardApplet::ClientState state, const QString &reasonCode);
    void snapshotChanged(const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot);
    void lockStateChanged(bool locked);
    void operationCompleted(quint64 requestId, const QindaQt::ShellClipboardApplet::OperationOutcome &outcome);
    void searchCompleted(quint64 requestId, const QindaQt::Services::ClipboardModel::SearchOutcome &outcome);
};

} // namespace QindaQt::ShellClipboardApplet
