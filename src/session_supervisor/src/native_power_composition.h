// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <memory>
namespace QindaQt::Platform::Compositor { class CompositorAttachment; }
namespace QindaQt::Power { class PowerClient; }
namespace QindaQt::Session::NativeLockRuntime { class Runtime; }
namespace QindaQt::Session::NativeSleep { class SleepCoordinator; }
namespace QindaQt::SessionSupervisor {
// Assembly only: collaborators remain borrowed on the supervisor Qt thread.
// This lifetime withdraws consumer declarations before releasing own causes.
class NativePowerComposition final {
public:
    NativePowerComposition(QDBusConnection bus, Platform::Compositor::CompositorAttachment &attachment,
        Power::PowerClient &power, Session::NativeLockRuntime::Runtime &lock,
        Session::NativeSleep::SleepCoordinator &sleep);
    ~NativePowerComposition();
    bool start();
    void stop();
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
