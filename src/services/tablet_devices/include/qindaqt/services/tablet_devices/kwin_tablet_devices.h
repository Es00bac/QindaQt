// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_device_port.h>

#include <QDBusConnection>

namespace QindaQt::Services::TabletDevices {

// Production adapter over KWin's org.kde.KWin input D-Bus API
// (org.kde.KWin.InputDeviceManager on /org/kde/KWin/InputDevice and
// org.kde.KWin.InputDevice device objects). KWin stays the live and
// persisted authority (ADR-0134); this adapter never writes config files.
//
// AGENT-NOTE: KWin 6.6 has no ListTablets, so the adapter enumerates the
// manager's `devicesSysNames` property and keeps the devices whose
// `tabletTool` or `tabletPad` property is true. Verified against KWin 6.6.6
// on 2026-09-17.
class KWinTabletDevicePort final : public TabletDevicePort {
public:
    explicit KWinTabletDevicePort(QDBusConnection bus);

    [[nodiscard]] QList<TabletDeviceSnapshot>
    devices(QString *error) const override;
    [[nodiscard]] bool device(const QString &deviceId,
                              TabletDeviceSnapshot *snapshot,
                              QString *error) const override;
    [[nodiscard]] bool writeProperty(const QString &deviceId,
                                     const QString &property,
                                     const QVariant &value,
                                     QString *error) const override;

private:
    QDBusConnection m_bus;
};

// Production hotplug adapter over
// org.kde.KWin.InputDeviceManager.deviceAdded/deviceRemoved.
class KWinTabletDeviceWatcher final : public TabletDeviceWatcher {
    Q_OBJECT
public:
    explicit KWinTabletDeviceWatcher(QDBusConnection bus,
                                     QObject *parent = nullptr);

    [[nodiscard]] bool start(QString *error) override;

private Q_SLOTS:
    void handleDeviceAdded(const QString &sysName);
    void handleDeviceRemoved(const QString &sysName);

private:
    QDBusConnection m_bus;
    bool m_started = false;
};

} // namespace QindaQt::Services::TabletDevices
