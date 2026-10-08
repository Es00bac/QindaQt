// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/qt_audio_transport.h>
#include <qindaqt/services/audio_protocol/audio_console.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <QtCore/QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    // No start(): linked public lifetime cannot activate a service or PipeWire.
    QindaQt::Audio::QtAudioTransport transport(
        QDBusConnection(QStringLiteral("audio-sdk-unused")));
    QindaQt::Audio::AudioClient client(&transport);
    const auto snapshot = client.snapshot();
    return client.state() == QindaQt::Audio::ClientState::Stopped
        && !client.hasSnapshot() && snapshot.devices.isEmpty() ? 0 : 1;
}
