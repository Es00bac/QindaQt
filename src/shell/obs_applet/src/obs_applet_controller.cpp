// SPDX-License-Identifier: GPL-3.0-or-later
#include "obs_applet_controller.h"

#include <QProcess>

namespace QindaQt::Shell::ObsApplet {

using namespace QindaQt::Obs;

ObsAppletController::ObsAppletController(ObsClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
    if (m_client != nullptr) {
        connect(m_client, &ObsClient::snapshotChanged, this,
                &ObsAppletController::republish);
        connect(m_client, &ObsClient::stateChanged, this,
                [this](ConnectionState, const QString &) { republish(); });
        connect(m_client, &ObsClient::operationFinished, this,
                [this](const ObsClient::OperationResult &result) {
                    if (result.ok) {
                        setFeedback(QString());
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
    const AppletModel next =
        projectApplet(m_client != nullptr ? m_client->snapshot() : ObsSnapshot{});
    if (next == m_model) {
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

bool ObsAppletController::dispatchable() {
    if (m_client != nullptr && m_model.controlAvailable) {
        return true;
    }
    setFeedback(m_model.unavailableText.isEmpty() ? tr("OBS is not running.")
                                                  : m_model.unavailableText);
    return false;
}

void ObsAppletController::toggleRecording() {
    if (!dispatchable()) {
        return;
    }
    setFeedback(QString());
    m_client->setOutputActive(OutputKind::Record, !m_model.recording);
}

void ObsAppletController::toggleStreaming() {
    if (!dispatchable()) {
        return;
    }
    setFeedback(QString());
    m_client->setOutputActive(OutputKind::Stream, !m_model.streaming);
}

void ObsAppletController::toggleVirtualCamera() {
    if (!dispatchable()) {
        return;
    }
    setFeedback(QString());
    m_client->setOutputActive(OutputKind::VirtualCam, !m_model.virtualCamera);
}

void ObsAppletController::selectScene(const QString &sceneName) {
    if (sceneName.isEmpty() || sceneName == m_model.currentScene) {
        return;
    }
    if (!dispatchable()) {
        return;
    }
    setFeedback(QString());
    m_client->setCurrentProgramScene(sceneName);
}

bool ObsAppletController::openObs() {
    // Detached, by desktop entry name, so OBS gets the session's own
    // environment rather than the shell's process group.
    const bool started = QProcess::startDetached(QStringLiteral("obs"), {});
    if (!started) {
        setFeedback(tr("OBS could not be started."));
    }
    return started;
}

} // namespace QindaQt::Shell::ObsApplet
