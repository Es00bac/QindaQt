// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller_plugin.h"
#include <input.h>
#include <core/session.h>
#include <main.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>
#include <QDBusMessage>
#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Controllers {
ControllerEndpoint::ControllerEndpoint(ControllerRuntime &runtime) : m_runtime(runtime) {
    connect(&runtime, &ControllerRuntime::changed, this, &ControllerEndpoint::Changed);
}
QString ControllerEndpoint::GetSnapshot() const { return m_runtime.snapshot(); }
QString ControllerEndpoint::Apply(const QString &id, const QString &patch, qulonglong revision) { return m_runtime.apply(id, patch, revision); }
QString ControllerEndpoint::Reset(const QString &id, qulonglong revision) { return m_runtime.resetProfile(id, revision); }
ControllerPlugin::ControllerPlugin()
    : m_runtime(m_input, QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).filePath("qindaqt/controllers.json")),
      m_endpoint(m_runtime) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(Service))) return;
    m_published = bus.registerObject(QString::fromLatin1(Object), &m_endpoint,
                                    QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    if (!m_published) {
        bus.unregisterService(QString::fromLatin1(Service));
        return;
    }
    KWin::input()->addInputDevice(&m_input);
    m_context.setInterval(250);
    connect(&m_context, &QTimer::timeout, this, &ControllerPlugin::refreshContext);
    connect(&m_input, &ControllerInputDevice::dictationFailed, this, [](const QString &) {
        auto notify = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
        notify.setArguments({"QindaQt", uint(0), "audio-input-microphone", "Controller dictation",
            "Dictation could not complete. Open Settings → Voice to check the microphone and provider.", QStringList{}, QVariantMap{}, 7000});
        QDBusConnection::sessionBus().asyncCall(notify);
    });
    QTimer::singleShot(0, this, [this] { refreshContext(); m_runtime.start(); m_context.start(); });
}
ControllerPlugin::~ControllerPlugin() {
    if (!m_published) return;
    m_context.stop();
    m_input.reset();
    const auto devices = KWin::input()->devices();
    for (auto it = m_touchpads.begin(); it != m_touchpads.end(); ++it)
        if (devices.contains(it.key())) it.key()->setEnabled(it.value());
    KWin::input()->removeInputDevice(&m_input);
    if (m_published) {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QString::fromLatin1(Object)); bus.unregisterService(QString::fromLatin1(Service));
    }
}
void ControllerPlugin::refreshContext() {
    QString reason;
    if (!KWin::kwinApp()->session()->isActive()) reason = "inactive-session";
    else if (KWin::waylandServer()->isScreenLocked()) reason = "locked";
    else if (KWin::workspace()->activeWindow() && KWin::workspace()->activeWindow()->isFullScreen()) reason = "fullscreen";
    m_runtime.setContext(reason.isEmpty(), reason);
    reconcileTouchpads();
}
void ControllerPlugin::reconcileTouchpads() {
    const auto devices = KWin::input()->devices();
    for (auto it = m_touchpads.begin(); it != m_touchpads.end();) {
        if (!devices.contains(it.key())) it = m_touchpads.erase(it); else ++it;
    }
    for (auto *device : devices) {
        // AGENT-GUARD: Sony controller touchpads also arrive through libinput.
        // Disable that duplicate desktop route, including while Steam owns
        // the pad; games still read their ungrabbed controller nodes normally.
        // The SDL controller route owns touchpad preference and relative motion.
        if (!device->isTouchpad() || device->vendor() != 0x054c) continue;
        if (!m_touchpads.contains(device)) m_touchpads[device] = device->isEnabled();
        if (device->isEnabled()) device->setEnabled(false);
    }
}
} // namespace QindaQt::Controllers
