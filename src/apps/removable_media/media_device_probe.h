// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QString>

namespace QindaQt::Apps::RemovableMedia {
// Read only this process's unique-owner export; never claim the production
// service name, connect to UDisks, or submit an operation. The caller owns the
// synthetic backend and mountedRoot until this bounded same-thread check ends.
int verifyDevicesContract(const QDBusConnection &owner, const QString &mountedRoot);
}
