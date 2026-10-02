// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <memory>
namespace QindaQt::Session::DisplayPower {
class DisplayPowerFacade;
// Borrows same-thread facade; owns only ScreenPower1 on the already-owned
// Session1 constructing connection. Current Power1 may submit one persistent
// lid episode; original caller/ID alone may cancel it after capability loss.
class ScreenPowerService final : public QObject {
    Q_OBJECT
public:
    ScreenPowerService(QDBusConnection bus, DisplayPowerFacade &facade, QObject *parent = nullptr);
    ~ScreenPowerService() override;
    bool start();
    void stop();
private:
    class Endpoint;
    QDBusConnection m_bus;
    DisplayPowerFacade &m_facade;
    QDBusServiceWatcher m_sessionWatch;
    std::unique_ptr<Endpoint> m_endpoint;
    bool m_active = false;
};
}
