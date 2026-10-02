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
      m_controlService(target == PortalBindingTarget::Resident ? QLatin1String(kNativePortalService) : QLatin1String(kNativeCapturePortalService)),
      m_controlPath(target == PortalBindingTarget::Resident ? QLatin1String(kNativePortalPath) : QLatin1String(kNativeCapturePortalPath)) {
    connect(&m_attachment, &QindaQt::Platform::Compositor::CompositorAttachment::revoked,
            this, &PortalSessionBinding::invalidated);
    connect(&m_attachment, &QindaQt::Platform::Compositor::CompositorAttachment::attached,
            this, &PortalSessionBinding::bindingChanged);
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
    m_attachment.revoke();
    if (!m_started) return;
    m_bus.unregisterService(m_controlService);
    m_bus.unregisterObject(m_controlPath);
    m_started = false;
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
