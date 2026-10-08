// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/bluetooth_radio_helper/radio_types.h>
#include <QtCore/QObject>
#include <functional>

namespace QindaQt::BluetoothRadio {
// Borrowed by the BlueZ backend and outlives it on the same Qt thread. Only an
// explicit power-on operation invokes observeAndUnblock. Returns an id before
// any completion is delivered; zero means no dispatch. Cancel suppresses later
// delivery but cannot undo a possible radio write. It never replays requests.
// Implementations may expose IPC but may not introduce ambient bus selection
// or pass an open radio descriptor to the caller. The borrowed same-thread
// current callback must be read-only/non-reentrant and remain valid until
// completion or cancel. Empty/throwing callback denies; cancel destroys it.
class RadioPowerPort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~RadioPowerPort() override = default;
    virtual quint64 observeAndUnblock(const QString &bluezOwner,
        const QString &adapterPath, const QString &adapterAddress,
        const QString &initiatingCaller, std::function<bool()> current) = 0;
    virtual void cancel(quint64 id) = 0;
Q_SIGNALS:
    void finished(quint64 id, const QindaQt::BluetoothRadio::Result &result);
};
} // namespace QindaQt::BluetoothRadio
