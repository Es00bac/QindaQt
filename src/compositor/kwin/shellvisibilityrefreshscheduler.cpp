// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellvisibilityrefreshscheduler.h"

#include <QTimer>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

ShellVisibilityRefreshScheduler::ShellVisibilityRefreshScheduler(
    Callback callback,
    QObject *parent)
    : QObject(parent)
    , m_callback(std::move(callback))
{
}

void ShellVisibilityRefreshScheduler::request()
{
    if (m_pending) {
        return;
    }
    m_pending = true;
    QTimer::singleShot(0, this, [this] { refresh(); });
}

bool ShellVisibilityRefreshScheduler::pending() const noexcept
{
    return m_pending;
}

void ShellVisibilityRefreshScheduler::refresh()
{
    m_pending = false;
    if (m_callback) {
        m_callback();
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
