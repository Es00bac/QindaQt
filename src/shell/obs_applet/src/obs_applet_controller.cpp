// SPDX-License-Identifier: GPL-3.0-or-later
#include "obs_applet_controller.h"

#include <QProcess>

namespace QindaQt::Shell::ObsApplet {

using namespace QindaQt::Obs;

ObsAppletController::ObsAppletController(ObsClient *client, QObject *parent,
                                       const bool controlGranted)
    : QObject(parent), m_client(client), m_controlGranted(controlGranted) {
    if (m_client != nullptr) {
        connect(m_client, &ObsClient::snapshotChanged, this,
                &ObsAppletController::republish);
        connect(m_client, &ObsClient::stateChanged, this,
                [this](ConnectionState, const QString &) { republish(); });
        connect(m_client, &ObsClient::operationFinished, this,
                [this](const ObsClient::OperationResult &result) {
                    if (!m_requests.remove(result.requestId)) {
                        return;
                    }
                    Q_EMIT stateChanged();
                    if (result.ok) {
                        setFeedback(QString());
                        return;
                    }
                    if (result.reasonCode == QLatin1String("obs-timeout")
                        || result.reasonCode == QLatin1String("obs-connection-lost")
                        || result.reasonCode == QLatin1String("obs-connection-replaced")) {
                        setFeedback(tr("OBS did not confirm the change. Check OBS before trying again."));
                        return;
                    }
                    setFeedback(result.comment.isEmpty()
                                    ? tr("OBS refused that (%1).")
                                          .arg(result.reasonCode)
                                    : result.comment);
                });
    }
    republish();
}

ObsAppletController::~ObsAppletController() = default;

void ObsAppletController::republish() {
    // AGENT-NOTE: a controller with no client is the shell's "OBS support is
    // not composed" state, and projecting the default snapshot gives it the
    // same honest "OBS is not running" presentation.
    AppletModel next =
        projectApplet(m_client != nullptr ? m_client->snapshot() : ObsSnapshot{});
    if (!m_controlGranted) {
        next.controlAvailable = false;
        next.unavailableText = tr("OBS controls are not permitted for this applet.");
    }
    const qsizetype pendingBefore = m_requests.size();
    if (!next.controlAvailable) {
        m_requests.clear();
    } else {
        // OBS may publish the requested state before its request response.
        // That observed change is enough to admit the opposite action.
        if (next.recording != m_model.recording)
            clearObservedRequests(QStringLiteral("record"));
        if (next.streaming != m_model.streaming)
            clearObservedRequests(QStringLiteral("stream"));
        if (next.virtualCamera != m_model.virtualCamera)
            clearObservedRequests(QStringLiteral("camera"));
        if (next.currentScene != m_model.currentScene)
            clearObservedRequests(QStringLiteral("scene"));
    }
    if (next == m_model && pendingBefore == m_requests.size()) {
        return;
    }
    m_model = next;
    Q_EMIT stateChanged();
}

void ObsAppletController::setFeedback(const QString &text) {
    if (m_feedback == text) {
        return;
    }
    m_feedback = text;
    Q_EMIT feedbackChanged();
}

bool ObsAppletController::actionAvailable(const QString &action) const {
    return m_client != nullptr && m_model.controlAvailable
        && !m_requests.values().contains(action);
}

bool ObsAppletController::recordingAvailable() const {
    return actionAvailable(QStringLiteral("record"));
}

bool ObsAppletController::streamingAvailable() const {
    return actionAvailable(QStringLiteral("stream"));
}

bool ObsAppletController::virtualCameraAvailable() const {
    return actionAvailable(QStringLiteral("camera"));
}

bool ObsAppletController::sceneAvailable() const {
    return actionAvailable(QStringLiteral("scene"));
}

QString ObsAppletController::pendingText() const {
    return m_requests.isEmpty() ? QString{} : tr("Waiting for OBS…");
}

void ObsAppletController::clearObservedRequests(const QString &action) {
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        if (it.value() == action)
            it = m_requests.erase(it);
        else
            ++it;
    }
}

void ObsAppletController::trackRequest(const quint64 requestId, const QString &action) {
    if (requestId == 0) {
        setFeedback(tr("OBS is busy. Try again in a moment."));
        return;
    }
    m_requests.insert(requestId, action);
    Q_EMIT stateChanged();
}

bool ObsAppletController::dispatchable(const QString &action) {
    if (actionAvailable(action)) {
        return true;
    }
    if (m_model.controlAvailable) {
        setFeedback(tr("Waiting for OBS to finish that change."));
        return false;
    }
    setFeedback(m_model.unavailableText.isEmpty() ? tr("OBS is not running.")
                                                  : m_model.unavailableText);
    return false;
}

void ObsAppletController::toggleRecording() {
    if (!dispatchable(QStringLiteral("record"))) {
        return;
    }
    setFeedback(QString());
    trackRequest(m_client->setOutputActive(OutputKind::Record, !m_model.recording),
                 QStringLiteral("record"));
}

void ObsAppletController::toggleStreaming() {
    if (!dispatchable(QStringLiteral("stream"))) {
        return;
    }
    setFeedback(QString());
    trackRequest(m_client->setOutputActive(OutputKind::Stream, !m_model.streaming),
                 QStringLiteral("stream"));
}

void ObsAppletController::toggleVirtualCamera() {
    if (!dispatchable(QStringLiteral("camera"))) {
        return;
    }
    setFeedback(QString());
    trackRequest(m_client->setOutputActive(OutputKind::VirtualCam, !m_model.virtualCamera),
                 QStringLiteral("camera"));
}

void ObsAppletController::selectScene(const QString &sceneName) {
    if (sceneName.isEmpty() || sceneName == m_model.currentScene) {
        return;
    }
    if (!dispatchable(QStringLiteral("scene"))) {
        return;
    }
    setFeedback(QString());
    trackRequest(m_client->setCurrentProgramScene(sceneName), QStringLiteral("scene"));
}

bool ObsAppletController::openObs() {
    // This launches the installed OBS executable in the session environment.
    const bool started = QProcess::startDetached(QStringLiteral("obs"), {});
    if (!started) {
        setFeedback(tr("OBS could not be started."));
    }
    return started;
}

bool ObsAppletController::openStreamingSettings() {
    const bool started = QProcess::startDetached(QStringLiteral("qindaqt-settings"),
                                                {QStringLiteral("--page"),
                                                 QStringLiteral("streaming")});
    if (!started) {
        setFeedback(tr("Streaming settings could not be opened."));
    }
    return started;
}

} // namespace QindaQt::Shell::ObsApplet
