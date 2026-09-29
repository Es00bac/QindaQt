// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_dialog_view_model.h"

namespace QindaQt::Apps::PolkitAgent {

PolkitDialogViewModel::PolkitDialogViewModel(QObject *parent) : QObject(parent) { }

void PolkitDialogViewModel::observe(PolkitRequestQueue &queue)
{
    connect(&queue, &PolkitRequestQueue::requestActivated, this,
           &PolkitDialogViewModel::attachRequest);
    connect(&queue, &PolkitRequestQueue::queueIdle, this, &PolkitDialogViewModel::detach);
}

void PolkitDialogViewModel::attachRequest(const AuthenticationRequest &request,
                                          PolkitAttemptController *controller)
{
    for (const auto &connection : m_connections) {
        disconnect(connection);
    }
    m_connections.clear();
    m_request = request;
    m_controller = controller;
    resetTransientState();
    m_busy = true;
    m_connections.push_back(connect(
        controller, &PolkitAttemptController::promptRequested, this,
        [this](const QString &text, bool echo) {
            m_promptText = text;
            m_promptEcho = echo;
            m_busy = false;
            Q_EMIT promptChanged();
            Q_EMIT busyChanged();
        }));
    m_connections.push_back(connect(
        controller, &PolkitAttemptController::infoMessage, this, [this](const QString &text) {
            m_infoText = text;
            Q_EMIT statusChanged();
        }));
    m_connections.push_back(connect(
        controller, &PolkitAttemptController::errorMessage, this, [this](const QString &text) {
            m_errorText = text;
            Q_EMIT statusChanged();
        }));
    m_connections.push_back(connect(
        controller, &PolkitAttemptController::attemptFailed, this, [this](const QString &text) {
            m_errorText = text;
            m_promptText.clear();
            m_busy = true;
            Q_EMIT statusChanged();
            Q_EMIT promptChanged();
            Q_EMIT busyChanged();
            Q_EMIT attemptFailed();
        }));
    m_connections.push_back(connect(controller, &PolkitAttemptController::completed, this,
                                    [this](bool gained) { Q_EMIT finished(gained); }));
    Q_EMIT requestChanged();
    Q_EMIT selectedIdentityIndexChanged();
    Q_EMIT promptChanged();
    Q_EMIT statusChanged();
    Q_EMIT busyChanged();
    Q_EMIT visibleChanged();
}

void PolkitDialogViewModel::detach()
{
    if (m_controller == nullptr) {
        return;
    }
    for (const auto &connection : m_connections) {
        disconnect(connection);
    }
    m_connections.clear();
    m_controller = nullptr;
    m_request = {};
    resetTransientState();
    Q_EMIT requestChanged();
    Q_EMIT visibleChanged();
}

void PolkitDialogViewModel::resetTransientState()
{
    m_promptText.clear();
    m_promptEcho = false;
    m_infoText.clear();
    m_errorText.clear();
    m_busy = false;
}

QString PolkitDialogViewModel::appName() const
{
    return m_request.requester.displayName.isEmpty() ? tr("An application")
                                                      : m_request.requester.displayName;
}

QStringList PolkitDialogViewModel::identityLabels() const
{
    QStringList labels;
    labels.reserve(m_request.identities.size());
    for (const auto &identity : m_request.identities) {
        labels.append(identity.displayLabel);
    }
    return labels;
}

int PolkitDialogViewModel::selectedIdentityIndex() const
{
    return m_controller ? m_controller->selectedIdentityIndex() : -1;
}

void PolkitDialogViewModel::selectIdentity(int index)
{
    if (m_controller == nullptr) {
        return;
    }
    m_controller->setSelectedIdentityIndex(index);
    Q_EMIT selectedIdentityIndexChanged();
}

void PolkitDialogViewModel::authenticate(const QString &response)
{
    if (m_controller == nullptr) {
        return;
    }
    // AGENT-GUARD: `response` is forwarded and never assigned to a member;
    // this class must never become a place a password outlives one call.
    m_busy = true;
    Q_EMIT busyChanged();
    m_controller->authenticate(response);
}

void PolkitDialogViewModel::cancel()
{
    if (m_controller == nullptr) {
        return;
    }
    m_controller->cancel();
}

} // namespace QindaQt::Apps::PolkitAgent
