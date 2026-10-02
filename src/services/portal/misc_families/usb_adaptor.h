// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_policy.h"
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
#include <functional>
namespace QindaQt::Services::Portal {
class UsbAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Usb")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    UsbAdaptor(QObject &, RequestRegistry &, MiscUi &);
    ~UsbAdaptor() override;
    uint version() const { return 1; }
public Q_SLOTS:
    quint32 AcquireDevices(const QDBusObjectPath &, const QString &, const QString &, const UsbDevices &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
private:
    RequestRegistry &m_requests; MiscUi &m_ui;
    QHash<RequestToken, UsbDevices> m_pending;
};
}
