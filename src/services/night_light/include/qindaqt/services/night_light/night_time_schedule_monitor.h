// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Services::NightLight {

// Watches the presence of the schedule authority (`org.kde.NightTime`,
// served by knighttimed). This is availability truth only: the monitor never
// calls Subscribe or any other changing method, never reads the schedule
// itself (KWin's own status stays the single live-state authority), and never
// starts the daemon.
class NightTimeScheduleMonitor : public QObject {
    Q_OBJECT

public:
    explicit NightTimeScheduleMonitor(QObject *parent = nullptr);
    ~NightTimeScheduleMonitor() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    // Fail closed: false until the daemon's name is observed on the bus.
    virtual bool scheduleAvailable() const = 0;

Q_SIGNALS:
    void scheduleAvailabilityChanged(bool available);
};

// Production implementation over an injected connection.
class QtNightTimeScheduleMonitor final : public NightTimeScheduleMonitor {
    Q_OBJECT

public:
    explicit QtNightTimeScheduleMonitor(const QDBusConnection &connection,
                                        QObject *parent = nullptr);
    ~QtNightTimeScheduleMonitor() override;

    void start() override;
    void stop() override;
    bool scheduleAvailable() const override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::NightLight
