// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

// Coalesces KWin signal bursts into one end-of-event-loop inventory sample.
// QObject context ownership cancels a queued callback during plugin unload.
class ShellVisibilityRefreshScheduler final : public QObject
{
public:
    using Callback = std::function<void()>;

    explicit ShellVisibilityRefreshScheduler(Callback callback,
                                             QObject *parent = nullptr);

    void request();
    [[nodiscard]] bool pending() const noexcept;

private:
    void refresh();

    Callback m_callback;
    bool m_pending = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
