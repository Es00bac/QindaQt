// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFlags>
#include <QObject>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

enum class HybridChromeSyncReason {
    Stacking = 0x1,
    Activation = 0x2,
    Outputs = 0x4,
    Windows = 0x8,
};
Q_DECLARE_FLAGS(HybridChromeSyncReasons, HybridChromeSyncReason)

// Coalesces workspace lifecycle bursts without retaining any KWin objects.
// The callback runs on this object's thread and is borrowed for this object's
// lifetime. Destruction cancels a queued callback through QObject context.
class HybridChromeSyncScheduler final : public QObject
{
public:
    using Callback = std::function<void(HybridChromeSyncReasons)>;

    explicit HybridChromeSyncScheduler(Callback callback,
                                       QObject *parent = nullptr);

    void stackingOrderChanged();
    void activeWindowChanged();
    void outputsChanged();
    void windowsChanged();

    [[nodiscard]] bool pending() const noexcept { return m_pending; }

private:
    void request(HybridChromeSyncReason reason);
    void synchronize();

    Callback m_callback;
    HybridChromeSyncReasons m_reasons;
    bool m_pending = false;
    bool m_synchronizing = false;
};

} // namespace QindaQt::Compositor::KWinIntegration

Q_DECLARE_OPERATORS_FOR_FLAGS(
    QindaQt::Compositor::KWinIntegration::HybridChromeSyncReasons)
