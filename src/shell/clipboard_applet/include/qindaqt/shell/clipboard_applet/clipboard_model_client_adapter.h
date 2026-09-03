// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include "qindaqt/shell/clipboard_applet/clipboard_client_interface.h"

namespace QindaQt::ShellClipboardApplet {

// AGENT-CONTRACT: Concrete in-memory adapter that binds ClipboardClientInterface
// to a ClipboardHistoryModel instance. It enforces fail-closed lock gating and
// generation fencing on the model seam without reaching into platform transports.
class ClipboardModelClientAdapter final : public ClipboardClientInterface {
    Q_OBJECT

public:
    explicit ClipboardModelClientAdapter(QindaQt::Services::ClipboardModel::ClipboardHistoryModel *model,
                                         QObject *parent = nullptr);
    ~ClipboardModelClientAdapter() override = default;

    [[nodiscard]] ClientState clientState() const noexcept override { return m_state; }
    [[nodiscard]] QString reasonCode() const override { return m_reasonCode; }
    [[nodiscard]] QString owner() const override { return m_owner; }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return m_ownerAvailable; }
    [[nodiscard]] bool isLocked() const noexcept override { return m_locked; }
    [[nodiscard]] QindaQt::Services::ClipboardModel::HistorySnapshot snapshot() const override;

    void setClientState(ClientState state, const QString &reasonCode = QString());
    void setOwner(const QString &owner, bool available = true);
    void setLocked(bool locked);
    // Independent host privacy authority (authenticated lock state, user
    // privacy policy, or a future host-side denial). Composed with the lock
    // below: privacy stays denied while EITHER the session is locked or the
    // host denies, so unlock never overrides an active host denial and a host
    // re-allow while locked never bypasses the lock.
    void setHostPrivacyDenied(bool denied);
    void notifyModelChanged();

    quint64 requestPromote(QindaQt::Services::ClipboardModel::EntryId id,
                           quint32 expectedGeneration,
                           quint64 tick) override;
    quint64 requestRemove(QindaQt::Services::ClipboardModel::EntryId id,
                          quint32 expectedGeneration) override;
    quint64 requestSetPinned(QindaQt::Services::ClipboardModel::EntryId id,
                             bool pinned,
                             quint32 expectedGeneration) override;
    quint64 requestClear(QindaQt::Services::ClipboardModel::ClearScope scope,
                         quint32 expectedGeneration) override;
    quint64 requestSearch(const QString &query,
                          quint32 expectedGeneration,
                          int maxResults) override;

private:
    [[nodiscard]] OperationOutcome mapClipboardError(QindaQt::Services::ClipboardModel::ClipboardError err,
                                                     QindaQt::Services::ClipboardModel::EntryId id) const;
    void applyPrivacyAuthority();

    QindaQt::Services::ClipboardModel::ClipboardHistoryModel *m_model = nullptr;
    ClientState m_state = ClientState::Ready;
    QString m_reasonCode;
    QString m_owner = QStringLiteral("org.qindaqt.ClipboardService");
    bool m_ownerAvailable = true;
    bool m_locked = false;
    // AGENT-GUARD: the model exposes one privacy bit, but the adapter composes
    // three denial causes over it. m_hostPrivacyDenied tracks the independent
    // host authority delivered through setHostPrivacyDenied(); m_foreignDenialAtLock
    // records a denial that was already active when the session locked (and
    // that neither the lock nor the tracked host flag caused) — the adapter
    // must never grant authority it did not deny itself, and unlock restores
    // exactly the denial the lock issued.
    bool m_hostPrivacyDenied = false;
    bool m_foreignDenialAtLock = false;
    quint64 m_nextRequestId = 1;
};

} // namespace QindaQt::ShellClipboardApplet
