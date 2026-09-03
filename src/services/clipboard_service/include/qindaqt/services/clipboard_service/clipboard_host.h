// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/services/clipboard_protocol/clipboard_protocol.h>
#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <QtCore/QObject>

namespace QindaQt::Services::Clipboard {

// Owns all payload bytes and history policy on one Qt thread. The borrowed
// adapter outlives the host. Settings and lock state enter only through the
// explicit gates; both default false and either false state purges C0.
class ClipboardHost final : public QObject,
                            public ClipboardWayland::CaptureObserver {
    Q_OBJECT
public:
    explicit ClipboardHost(ClipboardWayland::ClipboardWaylandAdapter *adapter,
                           quint64 epoch, QObject *parent = nullptr);
    ~ClipboardHost() override;
    [[nodiscard]] quint64 epoch() const noexcept { return m_epoch; }
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] OperationResult submit(const OperationRequest &request);
    void setHistoryOptIn(bool enabled);
    void setUnlocked(bool unlocked);

    void captureAvailabilityChanged(bool available) override;
    void captured(ClipboardWayland::SelectionKind kind,
                  const ClipboardModel::ClipboardValue &value) override;
    void captureRefused(ClipboardWayland::SelectionKind kind,
                        ClipboardModel::ClipboardError error) override;

Q_SIGNALS:
    void changed(quint64 epoch, quint32 generation, quint64 revision);

private:
    [[nodiscard]] OperationResult resultFor(const OperationRequest &request,
                                            OperationStatus status,
                                            const QString &reasonCode) const;
    void publishIfChanged(quint32 generation, quint64 revision,
                          bool enabled, bool allowed);
    [[nodiscard]] static QString reasonFor(ClipboardModel::ClipboardError error);

    ClipboardWayland::ClipboardWaylandAdapter *m_adapter = nullptr;
    ClipboardModel::ClipboardHistoryModel m_history;
    quint64 m_epoch = 0;
    quint64 m_tick = 0;
    bool m_optedIn = false;
    bool m_unlocked = false;
};

} // namespace QindaQt::Services::Clipboard
