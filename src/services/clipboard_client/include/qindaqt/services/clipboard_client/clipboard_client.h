// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_client/clipboard_transport.h>

#include <QtCore/QHash>
#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::Services::Clipboard {

// Exact-owner asynchronous Clipboard1 consumer. The borrowed transport must
// share this object's thread and outlive it. Snapshot publication is atomic;
// owner/epoch/generation/revision contradictions revoke all mutation authority.
class ClipboardClient final : public QObject {
    Q_OBJECT
public:
    explicit ClipboardClient(ClipboardTransport *transport, QObject *parent = nullptr);
    void start();
    void stop();
    void setRequestTimeout(int milliseconds);
    [[nodiscard]] ClientState state() const noexcept { return m_state; }
    [[nodiscard]] QString reasonCode() const { return m_reasonCode; }
    [[nodiscard]] QString owner() const { return m_owner; }
    [[nodiscard]] bool hasSnapshot() const noexcept { return m_snapshot.has_value(); }
    [[nodiscard]] Snapshot snapshot() const { return m_snapshot.value_or(Snapshot{}); }
    [[nodiscard]] bool operationPending() const noexcept { return m_operation.has_value(); }

    [[nodiscard]] quint64 select(const ClipboardModel::EntryId &entry);
    [[nodiscard]] quint64 remove(const ClipboardModel::EntryId &entry);
    [[nodiscard]] quint64 clear(bool all);
    [[nodiscard]] quint64 copy(const ClipboardModel::EntryId &entry);

Q_SIGNALS:
    void stateChanged(QindaQt::Services::Clipboard::ClientState state,
                      const QString &reasonCode);
    void snapshotChanged(const QindaQt::Services::Clipboard::Snapshot &snapshot);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Services::Clipboard::OperationResult &result);

private:
    struct Pending {
        quint64 token = 0;
        OperationRequest request;
    };
    void acceptOwner(const QString &owner);
    void acceptInvalidation(const QString &owner, quint64 epoch, quint32 generation,
                            quint64 revision);
    void acceptSnapshot(const QString &owner, quint64 token, bool transportSuccess,
                        const Snapshot &snapshot, const QString &reasonCode);
    void acceptOperation(const QString &owner, quint64 token, bool transportSuccess,
                         const OperationResult &result, const QString &reasonCode);
    void fetch();
    [[nodiscard]] quint64 begin(OperationKind kind,
                                const ClipboardModel::EntryId &entry, bool clearAll);
    void complete(OperationResult result);
    void completeUncertain(const QString &reasonCode);
    [[nodiscard]] OperationResult localResult(OperationKind kind, quint64 requestId,
                                              OperationStatus status,
                                              const QString &reasonCode) const;
    void publishState(ClientState state, const QString &reasonCode);

    ClipboardTransport *m_transport = nullptr;
    ClientState m_state = ClientState::Stopped;
    QString m_reasonCode;
    QString m_owner;
    std::optional<Snapshot> m_snapshot;
    std::optional<Pending> m_operation;
    QTimer m_fetchTimer;
    QTimer m_operationTimer;
    quint64 m_nextToken = 1;
    quint64 m_fetchToken = 0;
    int m_timeoutMs = 5'000;
    bool m_fetching = false;
    bool m_dirty = false;
};

} // namespace QindaQt::Services::Clipboard
