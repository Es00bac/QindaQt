// SPDX-License-Identifier: GPL-3.0-or-later
#include "record_controller.h"

#include <qindaqt/shell/obs_applet/obs_applet_presentation.h>

#include <QCoreApplication>

namespace QindaQt::Screenshot {
namespace {

// OBS finalizes a file between STOPPING and STOPPED; a long recording on a
// slow disk can take several seconds before it is confirmed.
constexpr int ConfirmMilliseconds = 15000;

} // namespace

RecordController::RecordController(Obs::ObsClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    m_confirm.setSingleShot(true);
    m_confirm.setInterval(ConfirmMilliseconds);
    connect(&m_confirm, &QTimer::timeout, this, [this] {
        if (m_pending == Pending::None)
            return;
        m_feedback = m_pending == Pending::Start
                         ? tr("OBS accepted the request but has not confirmed that it is recording.")
                         : tr("OBS accepted the request but has not confirmed that it stopped.");
        settle(false);
        recompute();
    });
    if (m_client) {
        connect(m_client, &Obs::ObsClient::snapshotChanged, this, &RecordController::handleSnapshot);
        connect(m_client, &Obs::ObsClient::stateChanged, this, &RecordController::handleSnapshot);
        connect(m_client, &Obs::ObsClient::operationFinished, this, &RecordController::handleOperation);
        // A file OBS reported before this controller existed is history, not
        // a new result to announce.
        m_lastPath = m_client->snapshot().lastRecordingPath;
    }
    recompute();
}

RecordController::~RecordController() = default;

void RecordController::setGateText(const QString &text)
{
    if (m_gateText == text)
        return;
    m_gateText = text;
    Q_EMIT changed();
}

QString RecordController::stateName() const
{
    switch (m_state) {
    case State::Unavailable:
        return QStringLiteral("unavailable");
    case State::Idle:
        return QStringLiteral("idle");
    case State::Starting:
        return QStringLiteral("starting");
    case State::Recording:
        return QStringLiteral("recording");
    case State::Paused:
        return QStringLiteral("paused");
    case State::Stopping:
        return QStringLiteral("stopping");
    }
    return {};
}

bool RecordController::canToggle() const
{
    return m_state == State::Idle || m_state == State::Recording || m_state == State::Paused;
}

QString RecordController::elapsed() const
{
    if (!m_client || !recording())
        return {};
    return Shell::ObsApplet::formatElapsed(m_client->snapshot().record.durationMs);
}

QString RecordController::statusText() const
{
    switch (m_state) {
    case State::Unavailable:
        if (!m_client)
            return tr("This build of QindaQt has no OBS integration.");
        if (!m_gateText.isEmpty())
            return m_gateText;
        return Shell::ObsApplet::projectApplet(m_client->snapshot()).unavailableText;
    case State::Idle:
        return tr("Ready to record the current OBS scene.");
    case State::Starting:
        return tr("Starting the recording…");
    case State::Recording:
        return tr("Recording");
    case State::Paused:
        return tr("Recording paused");
    case State::Stopping:
        return tr("Stopping the recording…");
    }
    return {};
}

bool RecordController::toggle()
{
    if (!m_client || !m_client->snapshot().ready()) {
        m_feedback = statusText();
        Q_EMIT changed();
        return false;
    }
    if (m_pending != Pending::None) {
        m_feedback = tr("Waiting for OBS…");
        Q_EMIT changed();
        return false;
    }
    const bool start = !m_client->snapshot().record.active;
    const quint64 id = m_client->setOutputActive(Obs::OutputKind::Record, start);
    if (id == 0) {
        // AGENT-GUARD: a zero id means nothing was sent; never enter a
        // pending state that no reply can ever settle.
        m_feedback = tr("OBS is busy. Try again in a moment.");
        Q_EMIT changed();
        return false;
    }
    m_pending = start ? Pending::Start : Pending::Stop;
    m_pendingId = id;
    m_feedback.clear();
    m_confirm.start();
    recompute();
    return true;
}

void RecordController::handleSnapshot()
{
    if (!m_client)
        return;
    const Obs::ObsSnapshot &snapshot = m_client->snapshot();
    if (snapshot.lastRecordingPath != m_lastPath) {
        m_lastPath = snapshot.lastRecordingPath;
        if (!m_lastPath.isEmpty())
            Q_EMIT recordingSaved(m_lastPath);
    }
    if (m_pending != Pending::None) {
        if (!snapshot.ready()) {
            m_feedback = tr("OBS went away before it confirmed the change.");
            settle(false);
        } else if ((m_pending == Pending::Start && snapshot.record.active)
                   || (m_pending == Pending::Stop && !snapshot.record.active)) {
            // A state event may confirm before the request's own reply.
            settle(true);
        }
    }
    recompute();
}

void RecordController::handleOperation(const Obs::ObsClient::OperationResult &result)
{
    if (m_pending == Pending::None || result.requestId != m_pendingId)
        return;
    if (result.ok) {
        // Accepted; the state event is the confirmation. Stop keeping the id
        // so a late duplicate reply cannot settle a later request.
        m_pendingId = 0;
        return;
    }
    m_feedback = result.comment.isEmpty() ? tr("OBS refused the request (%1).").arg(result.reasonCode)
                                          : result.comment;
    settle(false);
    recompute();
}

void RecordController::settle(bool ok)
{
    const bool started = m_pending == Pending::Start;
    m_pending = Pending::None;
    m_pendingId = 0;
    m_confirm.stop();
    if (ok)
        m_feedback.clear();
    Q_EMIT toggleFinished(ok, started);
}

void RecordController::recompute()
{
    State next = State::Unavailable;
    if (m_client && m_client->snapshot().ready()) {
        const Obs::OutputStatus &record = m_client->snapshot().record;
        if (record.active)
            next = m_pending == Pending::Stop ? State::Stopping
                   : record.paused            ? State::Paused
                                              : State::Recording;
        else
            next = m_pending == Pending::Start ? State::Starting : State::Idle;
    }
    m_state = next;
    // Elapsed time and sentences move without a state change.
    Q_EMIT changed();
}

} // namespace QindaQt::Screenshot
