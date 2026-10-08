// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "radio_operation_p.h"
#include <QtDBus/QDBusVirtualObject>

namespace QindaQt::BluetoothRadio {
// Private wire adapter. Sender comes only from the constructing bus; reentrant
// dispatch is refused before policy/platform calls. No automatic operation.
class RadioServiceObject final : public QDBusVirtualObject {
public:
    explicit RadioServiceObject(RadioOperation &operation);
    QString introspect(const QString &) const override;
    bool handleMessage(const QDBusMessage &, const QDBusConnection &) override;
private:
    RadioOperation &m_operation;
    bool m_busy = false;
};
}
