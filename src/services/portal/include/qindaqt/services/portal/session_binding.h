// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <QDBusContext>
#include <QDBusMessage>

namespace QindaQt::Services::Portal {
inline constexpr auto kNativePortalService = "org.qindaqt.Portal1";
inline constexpr auto kNativePortalPath = "/org/qindaqt/Portal1";
// Native composition control, never sandbox authority. The first same-UID
// session caller is retained; replacement callers cannot rebind it. Its exact
// liveness and display peer identity are proved by public attachment, not an
// ambient WAYLAND_DISPLAY or app/front-end payload. Same-thread only. All
// returned FDs are caller-owned; consumers must retire on invalidated().
class PortalSessionBinding final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Portal1")
public:
    PortalSessionBinding(QDBusConnection bus, QString runtimeDirectory,
                         QObject *parent = nullptr);
    bool start();
    void stop();
    bool live() const;
    int openDisplay();
    QString compositorOwner() const;
    bool admits(const QString &owner, quint64 pid) const;
    ~PortalSessionBinding() override;
public Q_SLOTS:
    Q_SCRIPTABLE bool AttachSessionWithDisplay(const QString &basename);
Q_SIGNALS:
    void invalidated();
    void bindingChanged();
private:
    QDBusConnection m_bus;
    QString m_sessionOwner;
    QindaQt::Platform::Compositor::CompositorAttachment m_attachment;
    bool m_started = false;
};
} // namespace QindaQt::Services::Portal
