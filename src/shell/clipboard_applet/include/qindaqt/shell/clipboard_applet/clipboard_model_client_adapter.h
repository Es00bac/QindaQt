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

    QindaQt::Services::ClipboardModel::ClipboardHistoryModel *m_model = nullptr;
    ClientState m_state = ClientState::Ready;
    QString m_reasonCode;
    QString m_owner = QStringLiteral("org.qindaqt.ClipboardService");
    bool m_ownerAvailable = true;
    bool m_locked = false;
    quint64 m_nextRequestId = 1;
};

} // namespace QindaQt::ShellClipboardApplet
