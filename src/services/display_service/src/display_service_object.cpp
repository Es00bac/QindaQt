// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_service_object_p.h"

#include <utility>

namespace QindaQt::DisplayService
{

DisplayServiceObject::DisplayServiceObject(
    DisplayServiceModel &model, std::function<void(bool)> transitionCallback,
    QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_transitionCallback(std::move(transitionCallback))
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
