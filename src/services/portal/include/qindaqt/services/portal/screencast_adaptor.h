// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/capture_ui.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <memory>
namespace QindaQt::Services::Portal {
// Same-thread registry/UI/bus outlive adaptor and its owned standard Sessions.
// Monitor sources, explicit single/multiple selection and all cursor modes;
// restore/window/virtual-source persistence remains a separate capability.
class ScreenCastAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")
    Q_PROPERTY(uint version READ version CONSTANT)
    Q_PROPERTY(uint AvailableSourceTypes READ availableSources CONSTANT)
    Q_PROPERTY(uint AvailableCursorModes READ availableCursors CONSTANT)
public:
    ScreenCastAdaptor(QObject &, RequestRegistry &, CaptureUI &, QDBusConnection);
    ~ScreenCastAdaptor() override;
    uint version() const { return 2; }
    uint availableSources() const { return 1; }
    uint availableCursors() const { return 7; }
public Q_SLOTS:
    quint32 CreateSession(const QDBusObjectPath &, const QDBusObjectPath &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 SelectSources(const QDBusObjectPath &, const QDBusObjectPath &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 Start(const QDBusObjectPath &, const QDBusObjectPath &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
private:
    class Private; std::unique_ptr<Private> d;
};
}
