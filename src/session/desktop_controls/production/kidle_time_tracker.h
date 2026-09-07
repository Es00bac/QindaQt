// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/session/desktop_controls/idle_tracker.h"

#include <QHash>

namespace QindaQt::Session::DesktopControls {

// Production idle observation over KIdleTime. The Wayland backend drives
// ext-idle-notify-v1 and honors idle inhibitors. Tokens map back to the armed
// millisecond value so the policy layer stays KIdleTime-free.
class KIdleTimeTracker final : public IdleTracker {
    Q_OBJECT

public:
    explicit KIdleTimeTracker(QObject *parent = nullptr);
    ~KIdleTimeTracker() override;

    KIdleTimeTracker(const KIdleTimeTracker &) = delete;
    KIdleTimeTracker &operator=(const KIdleTimeTracker &) = delete;

    void armTimeout(int milliseconds) override;
    void disarmTimeout(int milliseconds) override;

private:
    QHash<int, int> m_tokenForMilliseconds;
};

} // namespace QindaQt::Session::DesktopControls
