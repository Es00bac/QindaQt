// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/session_binding.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <unistd.h>
namespace QindaQt::Services::Portal {
PortalSessionBinding::PortalSessionBinding(QDBusConnection bus, QString runtimeDirectory, QObject *parent)
    : PortalSessionBinding(std::move(bus), std::move(runtimeDirectory), PortalBindingTarget::Resident, parent) {}
PortalSessionBinding::PortalSessionBinding(QDBusConnection bus, QString runtimeDirectory,
                                         PortalBindingTarget target, QObject *parent)
    : QObject(parent), m_bus(bus), m_attachment(bus, std::move(runtimeDirectory),
        [this](const QString &owner) { return !m_sessionOwner.isEmpty() && owner == m_sessionOwner; }),
      m_ownerWatch(QString{}, bus, QDBusServiceWatcher::WatchForUnregistration, this),
      m_controlService(target == PortalBindingTarget::Resident ? QLatin1String(kNativePortalService) : QLatin1String(kNativeCapturePortalService)),
      m_controlPath(target == PortalBindingTarget::Resident ? QLatin1String(kNativePortalPath) : QLatin1String(kNativeCapturePortalPath)) {
    connect(&m_attachment, &QindaQt::Platform::Compositor::CompositorAttachment::revoked,
            this, &PortalSessionBinding::invalidated);
    connect(&m_attachment, &QindaQt::Platform::Compositor::CompositorAttachment::attached,
            this, &PortalSessionBinding::bindingChanged);
    connect(&m_ownerWatch, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &name) {
                if (name == m_sessionOwner) retireSelectedOwner();
            });
}
PortalSessionBinding::~PortalSessionBinding() { stop(); }
bool PortalSessionBinding::start() {
    if (m_started) return true;
    if (!m_bus.registerObject(m_controlPath, this,
                             QDBusConnection::ExportScriptableSlots)) return false;
    if (!m_bus.registerService(m_controlService)) {
        m_bus.unregisterObject(m_controlPath); return false;
    }
    m_started = true; return true;
}
void PortalSessionBinding::stop() {
    if (!m_sessionOwner.isEmpty()) m_ownerWatch.removeWatchedService(m_sessionOwner);
    m_attachment.revoke();
    if (!m_started) return;
    m_bus.unregisterService(m_controlService);
    m_bus.unregisterObject(m_controlPath);
    m_started = false;
}
void PortalSessionBinding::retireSelectedOwner() {
    if (m_sessionOwnerLost) return;
    m_sessionOwnerLost = true;
    m_attachment.revoke();
    Q_EMIT selectedSessionOwnerLost();
}
bool PortalSessionBinding::AttachSessionWithDisplay(const QString &basename) {
    if (!calledFromDBus() || !connection().isConnected() || connection().name() != m_bus.name()) return false;
    const QString owner = message().service();
    const auto uid = m_bus.interface()->serviceUid(owner);
    if (!uid.isValid() || uid.value() != static_cast<uint>(geteuid()) || !owner.startsWith(QLatin1Char(':')))
        return false;
    if (!m_sessionOwner.isEmpty() && m_sessionOwner != owner) return false;
    // Retention occurs on explicit selection, including a failed display
    // attachment. Failure never grants a different caller the same control.
    m_sessionOwner = owner;
    // AGENT-GUARD: watch before rechecking the unique caller, closing the
    // vanish-between-UID-check-and-watch race. A lost first caller never
    // authorizes another caller in this resident process.
    m_ownerWatch.addWatchedService(owner);
    const auto stillOwned = m_bus.interface()->serviceOwner(owner);
    if (!stillOwned.isValid() || stillOwned.value() != owner) {
        retireSelectedOwner();
        return false;
    }
    return m_attachment.attach(owner, basename);
}
bool PortalSessionBinding::admits(const QString &owner, quint64 pid) const {
    const auto identity = m_attachment.identity();
    return m_started && identity && identity->compositorOwner == owner && identity->compositorPid == pid;
}
bool PortalSessionBinding::live() const { return m_started && m_attachment.live(); }
int PortalSessionBinding::openDisplay() { return live() ? m_attachment.openConnection() : -1; }
QString PortalSessionBinding::compositorOwner() const {
    const auto identity = m_attachment.identity();
    return identity ? identity->compositorOwner : QString{};
}
} // namespace QindaQt::Services::Portal
