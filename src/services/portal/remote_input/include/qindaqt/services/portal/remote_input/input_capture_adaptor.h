// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
// Adapted from xdg-desktop-portal-kde 6.6.6 src/inputcapture.h (9a5cc0e8).
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/services/portal/remote_input/compositor_eis.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QRect>
#include <memory>
#include <optional>

namespace QindaQt::Services::Portal::RemoteInput {
// Pure barrier rule shared with upstream: a barrier must fully cover one
// outer edge of exactly one zone. Returns the compositor-coordinate barrier
// (right/bottom edges moved onto the last pixel row/column) or nothing.
std::optional<QPair<QPoint, QPoint>> pointerBarrier(int x1, int y1, int x2, int y2, const QList<QRect> &zones);

// Standard org.freedesktop.impl.portal.InputCapture version 1. Same ownership
// and threading rules as RemoteDesktopAdaptor. CreateSession asks native
// consent before the compositor arms anything; zones come from the selected
// compositor, which also enforces barriers. Disabled/Activated/Deactivated/
// ZonesChanged are targeted to the session's frontend owner only.
class InputCaptureAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.InputCapture")
    Q_PROPERTY(uint version READ version CONSTANT)
    Q_PROPERTY(uint SupportedCapabilities READ supportedCapabilities CONSTANT)
public:
    InputCaptureAdaptor(QObject &host, RequestRegistry &, AccessConsent &, CompositorEis &, QDBusConnection);
    ~InputCaptureAdaptor() override;
    uint version() const { return 1; }
    uint supportedCapabilities() const { return kAllDeviceTypes; }
public Q_SLOTS:
    quint32 CreateSession(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &appId,
                          const QString &parentWindow, const QVariantMap &options, const QDBusMessage &call,
                          QVariantMap &results);
    quint32 GetZones(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &appId,
                     const QVariantMap &options, const QDBusMessage &call, QVariantMap &results);
    quint32 SetPointerBarriers(const QDBusObjectPath &handle, const QDBusObjectPath &session, const QString &appId,
                               const QVariantMap &options, const QList<QVariantMap> &barriers, uint zoneSet,
                               const QDBusMessage &call, QVariantMap &results);
    quint32 Enable(const QDBusObjectPath &session, const QString &appId, const QVariantMap &options,
                   const QDBusMessage &call, QVariantMap &results);
    quint32 Disable(const QDBusObjectPath &session, const QString &appId, const QVariantMap &options,
                    const QDBusMessage &call, QVariantMap &results);
    quint32 Release(const QDBusObjectPath &session, const QString &appId, const QVariantMap &options,
                    const QDBusMessage &call, QVariantMap &results);
    QDBusUnixFileDescriptor ConnectToEIS(const QDBusObjectPath &session, const QString &appId,
                                         const QVariantMap &options, const QDBusMessage &call);
Q_SIGNALS:
    // AGENT-NOTE: declared for standard introspection only. Delivery uses
    // targeted messages to the session's frontend; never Q_EMIT these, which
    // would broadcast activation and cursor data to every bus peer.
    void Disabled(const QDBusObjectPath &session, const QVariantMap &options);
    void Activated(const QDBusObjectPath &session, const QVariantMap &options);
    void Deactivated(const QDBusObjectPath &session, const QVariantMap &options);
    void ZonesChanged(const QDBusObjectPath &session, const QVariantMap &options);
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal::RemoteInput
