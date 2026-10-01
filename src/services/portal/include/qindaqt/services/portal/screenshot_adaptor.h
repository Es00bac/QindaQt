// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/capture_ui.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
namespace QindaQt::Services::Portal {
// Same-thread borrowed registry/UI outlive the adaptor. Own tokens only.
class ScreenshotAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Screenshot")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    ScreenshotAdaptor(QObject &, RequestRegistry &, CaptureUI &);
    ~ScreenshotAdaptor() override;
    uint version() const { return 2; }
public Q_SLOTS:
    quint32 Screenshot(const QDBusObjectPath &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 PickColor(const QDBusObjectPath &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
private:
    quint32 begin(bool color, const QDBusObjectPath &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    RequestRegistry &m_requests; CaptureUI &m_ui; QHash<RequestToken, CaptureKind> m_pending;
};
}
