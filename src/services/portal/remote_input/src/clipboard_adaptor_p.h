// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
// Method/session rules adapted from xdg-desktop-portal-kde 6.6.6
// src/clipboard.{h,cpp} (9a5cc0e8); KSystemClipboard and QMimeData copies are
// replaced by the compositor-owned EIS clipboard handle and direct paste FDs.
#pragma once
#include "remote_sessions_p.h"
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/services/portal/remote_input/compositor_eis.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

namespace QindaQt::Services::Portal::RemoteInput {
// Standard org.freedesktop.impl.portal.Clipboard version 1, owned by
// RemoteDesktopAdaptor on the same backend host and sharing its sessions.
// AGENT-GUARD: clipboard data moves only for a started RemoteDesktop session
// whose Start consent explicitly enabled the clipboard; the compositor handle
// is released on every session retirement. Paste FDs are one-shot.
class ClipboardAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Clipboard")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    ClipboardAdaptor(QObject &host, RemoteSessions &, AccessConsent &, CompositorEis &, QDBusConnection);
    ~ClipboardAdaptor() override;
    uint version() const { return 1; }
    // After explicit consent: opens the compositor handle; done(enabled) runs
    // once unless this adaptor is destroyed first.
    void enable(const QString &session, std::function<void(bool)> done);
    void retired(const RemoteSessions::Entry &entry);
public Q_SLOTS:
    void RequestClipboard(const QDBusObjectPath &session, const QVariantMap &options, const QDBusMessage &call);
    void SetSelection(const QDBusObjectPath &session, const QVariantMap &options, const QDBusMessage &call);
    QDBusUnixFileDescriptor SelectionWrite(const QDBusObjectPath &session, uint serial, const QDBusMessage &call);
    void SelectionWriteDone(const QDBusObjectPath &session, uint serial, bool success, const QDBusMessage &call);
    QDBusUnixFileDescriptor SelectionRead(const QDBusObjectPath &session, const QString &mimeType, const QDBusMessage &call);
Q_SIGNALS:
    // AGENT-NOTE: introspection only; delivery is targeted to the frontend.
    void SelectionOwnerChanged(const QDBusObjectPath &session, const QVariantMap &options);
    void SelectionTransfer(const QDBusObjectPath &session, const QString &mimeType, uint serial);
private:
    RemoteSessions::Entry *granted(const QDBusMessage &call, const QString &session);
    void refuse(const QDBusMessage &call, const char *error);
    void compositorSignal(const QDBusMessage &message);
    QString sessionForHandle(int handle) const;
    RemoteSessions &m_sessions;
    AccessConsent &m_consent;
    CompositorEis &m_eis;
    QDBusConnection m_bus;
    CompositorSignals m_sink;
    QString m_signalsOwner;
};
} // namespace QindaQt::Services::Portal::RemoteInput
