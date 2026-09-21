// SPDX-License-Identifier: GPL-3.0-or-later
#include "voice_route_composition.h"

#include <qindaqt/apps/settings_voice/voice_settings_model.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/voice_client/qt_voice_transport.h>
#include <qindaqt/services/voice_client/voice_client.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsVoice {

class VoiceRouteComposition::Private final {
public:
    Private()
        : settingsTransport(QDBusConnection::sessionBus())
        , settingsClient(settingsTransport,
                         {QString::fromLatin1(VoiceInputSettingsKey),
                          QString::fromLatin1(VoicePanelTranscriptSettingsKey)})
        , voiceTransport(QDBusConnection::sessionBus())
        , voiceClient(&voiceTransport)
        , model(settingsClient, voiceClient)
    {
        // AGENT-NOTE: like ClipboardRouteComposition, this QML singleton is the
        // route-local composition root. Both public clients keep their own
        // request-token domains and the model never sees a bus.
        QString ignoredError;
        const bool settingsStarted = settingsClient.start(&ignoredError);
        Q_UNUSED(settingsStarted);
        voiceClient.start();
    }

    Services::SettingsClient::QtSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settingsClient;
    Services::Voice::QtVoiceTransport voiceTransport;
    Services::Voice::VoiceClient voiceClient;
    VoiceSettingsModel model;
};

VoiceRouteComposition::VoiceRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
}

VoiceRouteComposition::~VoiceRouteComposition() = default;

QObject *VoiceRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsVoice
