// SPDX-License-Identifier: GPL-3.0-or-later
#include "dock_meter_lease.h"

#include "console_dock.h"

namespace QindaQt::ObsBridge {

DockMeterLease::DockMeterLease(Audio::AudioClient *client, ConsoleDock *dock, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(client != nullptr && dock != nullptr);
    connect(dock, &ConsoleDock::meterVisibilityChanged, this, &DockMeterLease::follow);
    // A destroyed dock shows nothing. Qt does not promise a hide event while a
    // widget is being destroyed, so the lease is not left to depend on one.
    connect(dock, &QObject::destroyed, this, [this] { m_lease.release(); });
    // The dock may already be on screen when it meets this object.
    follow(dock->isVisible());
}

DockMeterLease::~DockMeterLease() = default;

void DockMeterLease::follow(bool visible)
{
    if (!visible) {
        m_lease.release();
    } else if (!m_lease.held()) {
        m_lease = Audio::LevelsLease(m_client);
    }
}

} // namespace QindaQt::ObsBridge
