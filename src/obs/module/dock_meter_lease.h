// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_client/audio_client.h>

#include <QObject>

namespace QindaQt::ObsBridge {

class ConsoleDock;

// AGENT-CONTRACT (ADR-0394): holds one Audio1 meter lease on the client for
// exactly as long as the console dock is visible. The Levels stream is the one
// high-rate signal on the bus (twenty frames a second), and a dock that is
// closed, tabbed behind another or on a hidden window shows no meter, so
// nothing should be subscribed, received or decoded for it. The lease is taken
// when the dock becomes visible (or when this object meets a dock that already
// is) and given back when it is hidden, when the dock is destroyed (OBS owns a
// registered dock and may destroy it first) and when this object is destroyed.
//
// This is deliberately a small collaborator with constructor-visible
// dependencies rather than bridge logic: it needs no libobs, so its lease
// behavior is tested without an OBS frontend or a running libobs.
//
// Ownership and lifetime: `client` and `dock` are borrowed. The client must
// outlive this object (BridgeController destroys it first). Threading: the Qt
// main thread, like the dock and the client.
//
// Known limit: QWidget visibility does not change when the OBS window is
// minimized, so a minimized OBS keeps its lease until the dock is hidden. The
// QML surfaces of Settings and the shell do track minimization.
class DockMeterLease final : public QObject {
    Q_OBJECT

public:
    DockMeterLease(Audio::AudioClient *client, ConsoleDock *dock, QObject *parent = nullptr);
    ~DockMeterLease() override;

    [[nodiscard]] bool held() const noexcept { return m_lease.held(); }

private:
    void follow(bool visible);

    Audio::AudioClient *m_client;
    Audio::LevelsLease m_lease;
};

} // namespace QindaQt::ObsBridge
