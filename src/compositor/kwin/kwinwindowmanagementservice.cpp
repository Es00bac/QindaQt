// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinwindowmanagementservice.h"
#include "kwinwindowmanagementruntime.h"
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/voice_preferences/voice_input_preference_gate.h>
#include <qindaqt/services/voice_protocol/voice_settings_keys.h>
#include <qindaqt/window_management/qt_command_endpoint.h>
#include <qindaqt/window_management/qt_voice_command_authority.h>
#include <wayland_server.h>
namespace QindaQt::Compositor::KWinIntegration {
class KWinWindowManagementService::Private {
public:
  Private(ManagedWindowRegistry &registry, KWinHybridSession &session,
          QDBusConnection connection)
      : bus(std::move(connection)), transport(bus),
        settings(transport, {QString::fromLatin1(
                                Services::Voice::kVoiceInputSettingsKey)}),
        gate(settings),
        authority(gate, bus,
                  [] {
                    return !KWin::waylandServer() ||
                           KWin::waylandServer()->isScreenLocked();
                  }),
        runtime(registry, session,
                [this] { return authority.providerProcessId(); }),
        endpoint(authority, runtime, runtime) {
    QObject::connect(
        &authority, &WindowManagement::QtVoiceCommandAuthority::invalidated,
        &endpoint, &WindowManagement::QtCommandEndpoint::invalidate);
    if (KWin::waylandServer())
      QObject::connect(KWin::waylandServer(),
                       &KWin::WaylandServer::lockStateChanged, &endpoint,
                       &WindowManagement::QtCommandEndpoint::invalidate);
  }
  QDBusConnection bus;
  Services::SettingsClient::QtSettingsTransport transport;
  Services::SettingsClient::SettingsClient settings;
  Services::VoicePreferences::VoiceInputPreferenceGate gate;
  WindowManagement::QtVoiceCommandAuthority authority;
  KWinWindowManagementRuntime runtime;
  WindowManagement::QtCommandEndpoint endpoint;
  bool registered = false;
};
KWinWindowManagementService::KWinWindowManagementService(
    ManagedWindowRegistry &registry, KWinHybridSession &session,
    QDBusConnection connection, QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(registry, session, std::move(connection))) {}
bool KWinWindowManagementService::start() {
  if (d->registered)
    return true;
  if (!d->settings.start())
    return false;
  d->registered = d->bus.registerObject(
      QString::fromLatin1(WindowManagement::CommandObjectPath), &d->endpoint,
      QDBusConnection::ExportScriptableSlots);
  if (!d->registered)
    d->settings.stop();
  return d->registered;
}
KWinWindowManagementService::~KWinWindowManagementService() {
  d->endpoint.invalidate();
  if (d->registered)
    d->bus.unregisterObject(
        QString::fromLatin1(WindowManagement::CommandObjectPath));
  d->settings.stop();
}
} // namespace QindaQt::Compositor::KWinIntegration
