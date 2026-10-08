// SPDX-License-Identifier: LGPL-3.0-or-later
#include "radio_service_session_p.h"
#include <QtCore/QThread>
#include <QtCore/QUuid>

namespace QindaQt::BluetoothRadio {
RadioServiceSession::RadioServiceSession(const QString &address) : d(std::make_unique<Private>()) {
    bool suppliedGuid = false;
    if (!NativeRadioWire::boundedAddress(address, &suppliedGuid)) {
        // Missing composition can retain legacy startup; a supplied malformed
        // address must not be reinterpreted by a more permissive fallback.
        d->fallbackAllowed = address.isEmpty();
        return;
    }
    d->wire = std::make_unique<NativeRadioWire>();
    const bool opened = d->wire->open(address);
    d->fallbackAllowed = !d->wire->selectedPeer();
    if (!opened) return;
    d->fallbackAllowed = false;
    d->connectionName = QStringLiteral("radio-authority-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    // AGENT-GUARD: libdbus authentication checks this GUID against the server;
    // two independent opens may never silently span a replaced bus (ADR0359).
    d->authority = QDBusConnection::connectToBus(d->wire->pinnedAddress(), d->connectionName);
    if (!d->authority.isConnected() || !d->wire->connected()) {
        d->reason = QStringLiteral("radio-stale-target");
        return;
    }
    d->ready = true;
    d->reason.clear();
}
RadioServiceSession::~RadioServiceSession() {
    if (!d->connectionName.isEmpty()) QDBusConnection::disconnectFromBus(d->connectionName);
}
bool RadioServiceSession::prepared() const {
    return d->ready && d->wire && d->wire->thread() == QThread::currentThread()
        && d->wire->connected() && d->authority.isConnected();
}
bool RadioServiceSession::legacyStartupAllowed() const { return !d->ready && d->fallbackAllowed; }
QDBusConnection RadioServiceSession::authorityConnection() const { return d->authority; }
QString RadioServiceSession::reasonCode() const {
    if (prepared()) return {};
    return d->ready ? QStringLiteral("radio-stale-target") : d->reason;
}
} // namespace QindaQt::BluetoothRadio
