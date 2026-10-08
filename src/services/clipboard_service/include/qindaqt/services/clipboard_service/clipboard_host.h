// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/services/clipboard_protocol/clipboard_protocol.h>
#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <QtCore/QObject>

#include <functional>
#include <memory>

namespace QindaQt::Services::Clipboard {

class ClipboardPrivacyState;

// Borrowed same-thread, synchronous, read-only and non-reentrant admission.
// Captures outlive the host. Empty/throwing/reentrant admission denies privacy,
// purges history and cancels capture. Production supplies live independently
// authenticated native attachment/lock proof; setUnlocked(true) alone is not
// authority. No callback is invoked after host destruction.
using PrivacyAdmission = std::function<bool()>;

// Owns payload bytes and history policy on one Qt thread. The borrowed adapter
// and admission dependencies outlive the host. Consent and privacy default
// false; either false purges content and revokes prior entry lineage.
// Source-compatible legacy construction is for explicitly gated test/legacy
// compositions; native production must use the admission-taking overload.
class ClipboardHost final : public QObject,
                            public ClipboardWayland::CaptureObserver {
    Q_OBJECT
public:
    explicit ClipboardHost(ClipboardWayland::ClipboardWaylandAdapter *adapter,
                           quint64 epoch, QObject *parent = nullptr);
    ClipboardHost(ClipboardWayland::ClipboardWaylandAdapter *adapter,
                  quint64 epoch, PrivacyAdmission admission, QObject *parent = nullptr);
    ~ClipboardHost() override;
    [[nodiscard]] quint64 epoch() const noexcept { return m_epoch; }
    // Also reconciles synchronous authority loss: an event-loop-delayed signal
    // cannot disclose retained descriptors. Reconciliation may emit changed().
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
    void publishIfChanged(const ClipboardModel::HistorySnapshot &before);
    [[nodiscard]] bool operationAdmitted(quint32 generation);
    [[nodiscard]] static QString reasonFor(ClipboardModel::ClipboardError error);

    ClipboardWayland::ClipboardWaylandAdapter *m_adapter = nullptr;
    std::unique_ptr<ClipboardPrivacyState> m_privacy;
    quint64 m_epoch = 0;
    quint64 m_tick = 0;
};

} // namespace QindaQt::Services::Clipboard
