// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_service_object_p.h"

#include <QtCore/QVariant>

#include <algorithm>
#include <utility>

namespace QindaQt::DisplayService
{

DisplayServiceObject::DisplayServiceObject(
    DisplayServiceModel &model, std::function<void(bool)> transitionCallback,
    std::function<void()> brightnessCallback, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_transitionCallback(std::move(transitionCallback))
    , m_brightnessCallback(std::move(brightnessCallback))
{
}

Display::Snapshot DisplayServiceObject::GetSnapshot()
{
    if (const Display::Snapshot *snapshot = m_model.snapshot(); snapshot != nullptr) {
        return *snapshot;
    }
    unavailableReply();
    return {};
}

Display::OperationResult DisplayServiceObject::Stage(
    const QString &transactionId, const Display::Candidate &candidate)
{
    return complete(m_model.stage(transactionId, candidate));
}

Display::OperationResult DisplayServiceObject::Preview(const QString &transactionId)
{
    return complete(m_model.preview(transactionId));
}

Display::OperationResult DisplayServiceObject::Confirm(const QString &transactionId)
{
    return complete(m_model.confirm(transactionId));
}

Display::OperationResult DisplayServiceObject::Cancel(const QString &transactionId)
{
    return complete(m_model.cancel(transactionId));
}

Display::BrightnessSnapshot DisplayServiceObject::GetBrightness()
{
    if (const Display::BrightnessSnapshot *brightness = m_model.brightnessSnapshot();
        brightness != nullptr) {
        return *brightness;
    }
    unavailableReply();
    return {};
}

Display::OperationResult DisplayServiceObject::SetOutputBrightness(
    const Display::BrightnessRequest &request)
{
    const BrightnessRequestResult result = m_model.setOutputBrightness(request);
    if (!result.available) {
        unavailableReply();
        return {};
    }
    if (!result.final && calledFromDBus()) {
        // AGENT-CONTRACT: An accepted immediate request replies only when the
        // model finishes it, after the republish that proves Applied.
        setDelayedReply(true);
        m_brightnessReplies.push_back(
            {.requestId = result.requestId, .connection = connection(), .call = message()});
    }
    if (m_brightnessCallback) {
        m_brightnessCallback();
    }
    return result.operation;
}

void DisplayServiceObject::finishBrightness(const BrightnessFinish &finish)
{
    const auto found = std::ranges::find(m_brightnessReplies, finish.requestId,
                                         &DelayedReply::requestId);
    if (found == m_brightnessReplies.end()) {
        return;
    }
    const DelayedReply reply = std::move(*found);
    m_brightnessReplies.erase(found);
    reply.connection.send(reply.call.createReply(QVariant::fromValue(finish.operation)));
}

Display::OperationResult DisplayServiceObject::complete(
    const ServiceOperationResult &result)
{
    if (!result.available) {
        unavailableReply();
        return {};
    }
    if (m_transitionCallback) {
        m_transitionCallback(result.command.stateChanged);
    }
    if (result.command.stateChanged) {
        notifyChanged();
    }
    return result.operation;
}

void DisplayServiceObject::notifyChanged()
{
    const Display::Snapshot *snapshot = m_model.snapshot();
    if (snapshot == nullptr) {
        Q_EMIT Changed({}, 0, false);
        return;
    }
    Q_EMIT Changed(snapshot->serviceEpoch, snapshot->revision, true);
}

void DisplayServiceObject::unavailableReply()
{
    if (calledFromDBus()) {
        sendErrorReply(QStringLiteral("org.qindaqt.Display1.Error.Unavailable"),
                       QStringLiteral("no accepted display inventory is available"));
    }
}

} // namespace QindaQt::DisplayService
