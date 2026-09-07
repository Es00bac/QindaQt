// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// Seam over compositor idle observation. Production uses KIdleTime, whose
// Wayland backend drives ext-idle-notify-v1 and therefore honors idle
// inhibitors taken by video players and similar clients.
class IdleTracker : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~IdleTracker() override = default;

    IdleTracker(const IdleTracker &) = delete;
    IdleTracker &operator=(const IdleTracker &) = delete;

    virtual void armTimeout(int milliseconds) = 0;
    virtual void disarmTimeout(int milliseconds) = 0;

Q_SIGNALS:
    void timeoutReached(int milliseconds);
    void resumingFromIdle();
};

} // namespace QindaQt::Session::DesktopControls
