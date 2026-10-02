// SPDX-License-Identifier: LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2018 Red Hat Inc
// SPDX-FileCopyrightText: 2018 Jan Grulich <jgrulich@redhat.com>
// SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
// Adapted from xdg-desktop-portal-kde 6.6.6 src/remotedesktop.h (9a5cc0e8).
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/services/portal/capture_ui.h>
#include <qindaqt/services/portal/remote_input/compositor_eis.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <memory>

namespace QindaQt::Services::Portal::RemoteInput {
// Standard org.freedesktop.impl.portal.RemoteDesktop version 2 on the public
// backend host. Configure before the host's ExportAdaptors registration. The
// borrowed registry, consent and EIS port must outlive this same-thread
// adaptor; destruction closes its sessions and disconnects their EIS contexts.
// Input reaches the compositor only through ConnectToEIS after explicit native
// consent. Notify* calls fail with org.freedesktop.DBus.Error.NotSupported:
// this backend owns no second, non-EIS injection path. The frontend sends them
// without awaiting a reply, so legacy callers see no error and get no input.
// With a capture port, screenCastSources() accepts the ScreenCast selection for
// these sessions and Start publishes the producer's streams after its consent.
class RemoteDesktopAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.RemoteDesktop")
    Q_PROPERTY(uint version READ version CONSTANT)
    Q_PROPERTY(uint AvailableDeviceTypes READ availableDeviceTypes CONSTANT)
public:
    RemoteDesktopAdaptor(QObject &host, RequestRegistry &, AccessConsent &, CompositorEis &, QDBusConnection);
    // Borrowed capture port (outlives this adaptor) for combined sessions.
    RemoteDesktopAdaptor(QObject &host, RequestRegistry &, AccessConsent &, CompositorEis &, CaptureUI &, QDBusConnection);
    ~RemoteDesktopAdaptor() override;
    // Lent to ScreenCastAdaptor; valid for this adaptor's lifetime.
    ScreenCastSourceDelegate &screenCastSources();
    uint version() const { return 2; }
    uint availableDeviceTypes() const { return kAllDeviceTypes; }
public Q_SLOTS:
    quint32 CreateSession(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &appId,
                          const QVariantMap &options, const QDBusMessage &call, QVariantMap &results);
    quint32 SelectDevices(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &appId,
                          const QVariantMap &options, const QDBusMessage &call, QVariantMap &results);
    quint32 Start(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &appId,
                  const QString &parentWindow, const QVariantMap &options, const QDBusMessage &call, QVariantMap &results);
    QDBusUnixFileDescriptor ConnectToEIS(const QDBusObjectPath &session, const QString &appId,
                                         const QVariantMap &options, const QDBusMessage &call);
    void NotifyPointerMotion(const QDBusObjectPath &, const QVariantMap &, double, double, const QDBusMessage &);
    void NotifyPointerMotionAbsolute(const QDBusObjectPath &, const QVariantMap &, uint, double, double, const QDBusMessage &);
    void NotifyPointerButton(const QDBusObjectPath &, const QVariantMap &, int, uint, const QDBusMessage &);
    void NotifyPointerAxis(const QDBusObjectPath &, const QVariantMap &, double, double, const QDBusMessage &);
    void NotifyPointerAxisDiscrete(const QDBusObjectPath &, const QVariantMap &, uint, int, const QDBusMessage &);
    void NotifyKeyboardKeycode(const QDBusObjectPath &, const QVariantMap &, int, uint, const QDBusMessage &);
    void NotifyKeyboardKeysym(const QDBusObjectPath &, const QVariantMap &, int, uint, const QDBusMessage &);
    void NotifyTouchDown(const QDBusObjectPath &, const QVariantMap &, uint, uint, double, double, const QDBusMessage &);
    void NotifyTouchMotion(const QDBusObjectPath &, const QVariantMap &, uint, uint, double, double, const QDBusMessage &);
    void NotifyTouchUp(const QDBusObjectPath &, const QVariantMap &, uint, const QDBusMessage &);
private:
    RemoteDesktopAdaptor(QObject &host, RequestRegistry &, AccessConsent &, CompositorEis &, CaptureUI *, QDBusConnection);
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal::RemoteInput
