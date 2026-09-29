// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_client.h>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

namespace QindaQt::Screenshot {

// Start and stop an OBS recording, as a small state machine over the
// desktop's one obs-websocket client (ADR-0201).
//
// AGENT-CONTRACT: the state is derived from OBS's own snapshot plus the one
// request this controller has outstanding; nothing is assumed from a button
// press. A paused recording is still a recording (the client preserves
// `outputState`, not just `outputActive`). A refused request is reported in
// OBS's words, and a request OBS never confirms is reported as unconfirmed.
// Unavailable sentences are the OBS applet's (projectApplet), so the chip
// and this tool never disagree about why OBS cannot be driven.
class RecordController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ stateName NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool canToggle READ canToggle NOTIFY changed)
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(QString elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString feedback READ feedback NOTIFY changed)
    Q_PROPERTY(QString lastRecordingPath READ lastRecordingPath NOTIFY changed)

public:
    enum class State { Unavailable, Idle, Starting, Recording, Paused, Stopping };
    Q_ENUM(State)

    // `client` may be null (no OBS integration built or granted): the
    // controller then stays Unavailable and says why.
    explicit RecordController(Obs::ObsClient *client, QObject *parent = nullptr);
    ~RecordController() override;

    // Sets the sentence used while the connection gate is closed, so the
    // user learns "set OBS up" rather than "OBS is not running".
    void setGateText(const QString &text);
    // How long an accepted request may wait for OBS's state event before it
    // is reported as unconfirmed. Rows shorten it; production keeps 15 s.
    void setConfirmTimeout(int milliseconds) { m_confirm.setInterval(milliseconds); }

    // Starts when idle, stops when recording or paused. Returns false and
    // sets feedback when nothing was sent.
    Q_INVOKABLE bool toggle();

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] QString stateName() const;
    [[nodiscard]] bool available() const { return m_state != State::Unavailable; }
    [[nodiscard]] bool canToggle() const;
    [[nodiscard]] bool recording() const
    {
        return m_state == State::Recording || m_state == State::Paused || m_state == State::Stopping;
    }
    [[nodiscard]] bool paused() const { return m_state == State::Paused; }
    [[nodiscard]] QString elapsed() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString feedback() const { return m_feedback; }
    [[nodiscard]] QString lastRecordingPath() const { return m_lastPath; }

Q_SIGNALS:
    void changed();
    // The transition this controller asked for was confirmed (true) or
    // refused/unconfirmed (false). Exactly once per successful toggle().
    void toggleFinished(bool ok, bool started);
    // OBS reported a finished recording's file, whoever stopped it.
    void recordingSaved(const QString &path);

private:
    enum class Pending { None, Start, Stop };

    void handleSnapshot();
    void handleOperation(const Obs::ObsClient::OperationResult &result);
    void settle(bool ok);
    void recompute();

    QPointer<Obs::ObsClient> m_client;
    State m_state = State::Unavailable;
    Pending m_pending = Pending::None;
    quint64 m_pendingId = 0;
    QTimer m_confirm;
    QString m_feedback;
    QString m_gateText;
    QString m_lastPath;
};

} // namespace QindaQt::Screenshot
