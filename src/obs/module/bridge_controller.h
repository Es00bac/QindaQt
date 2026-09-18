// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/obs_bridge/console_sources.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <obs-frontend-api.h>

#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::Audio {
class QtAudioTransport;
}

namespace QindaQt::ObsBridge {

class ConsoleDock;

// The bridge's published view: what obs-websocket clients read.
struct ConsoleMapping {
    QList<DesiredSource> sources;
    QString audioState = QStringLiteral("stopped");
    QString reasonCode;
    quint64 epoch = 0;
    quint64 revision = 0;
};

// AGENT-CONTRACT: the libobs half of the bridge (ADR-0208). It subscribes to
// Audio1 through the shared audio client on OBS's Qt main thread, and on
// every snapshot makes the OBS source set equal to the console: creating,
// renaming, retargeting and removing the two bridge source types and
// attaching each to a free mixer channel from 6 up (the frontend owns 1-5).
// With a frontend present it waits for the scene collection to finish
// loading, since loading replaces every source; without one (tests) it
// applies immediately. It never mutates the console: mute mirroring and
// macros belong to the obs-websocket client (O10).
class BridgeController final : public QObject {
    Q_OBJECT

public:
    explicit BridgeController(QObject *parent = nullptr);
    ~BridgeController() override;

    // Connects to Audio1 on the session bus, adds the dock when a frontend
    // exists, and starts syncing once the frontend has finished loading.
    void start();
    void stop();

    // Makes OBS's source set equal to this snapshot now. Public so the
    // libobs row can drive the sync without D-Bus.
    void applySnapshot(const Audio::Snapshot &snapshot);

    [[nodiscard]] ConsoleMapping mapping() const;
    [[nodiscard]] bool frontendReady() const noexcept { return m_frontendReady; }

    // The first mixer channel the bridge uses; the frontend owns 1-5.
    static constexpr uint32_t FirstChannel = 6;

Q_SIGNALS:
    void mappingChanged();

private:
    static void frontendEvent(enum obs_frontend_event event, void *data);
    void onSnapshot(const Audio::Snapshot &snapshot);
    void onState(Audio::ClientState state, const QString &reasonCode);
    void onLevels(const QList<Audio::LevelReading> &levels);

    std::unique_ptr<Audio::QtAudioTransport> m_transport;
    std::unique_ptr<Audio::AudioClient> m_client;
    ConsoleDock *m_dock = nullptr;
    bool m_frontendCallbacks = false;
    bool m_frontendReady = true;
    bool m_haveSnapshot = false;
    Audio::Snapshot m_lastSnapshot;
    mutable QMutex m_mutex;
    ConsoleMapping m_mapping;
};

} // namespace QindaQt::ObsBridge
