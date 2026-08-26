// SPDX-License-Identifier: LGPL-3.0-or-later

#include "notification_capacity_p.h"

#include "notification_validation_p.h"

#include <QtGlobal>

#include <utility>

namespace QindaQt::Services::Notifications::Private {
namespace {

SubmissionCapacityEvaluation reject(QString error)
{
    SubmissionCapacityEvaluation evaluation;
    evaluation.error = std::move(error);
    return evaluation;
}

} // namespace

NotificationCapacityLedger::NotificationCapacityLedger(NotificationPolicy policy)
    : m_policy(std::move(policy))
{
}

SubmissionCapacityEvaluation NotificationCapacityLedger::evaluateSubmission(
    const NotificationRequest &request,
    bool replacing,
    qsizetype previousPayloadBytes) const
{
    const SourceUsage usage = m_sourceUsage.value(request.sourceService);
    Q_ASSERT(previousPayloadBytes >= 0);
    Q_ASSERT(previousPayloadBytes <= usage.retainedPayloadBytes);
    Q_ASSERT(previousPayloadBytes <= m_retainedPayloadBytes);
    if (!replacing
        && usage.activeCount >= m_policy.maximumActiveNotificationsPerSource) {
        return reject(QStringLiteral(
            "the source's bounded active-notification capacity is full"));
    }
    if (!replacing && m_activeCount >= m_policy.maximumActiveNotifications) {
        return reject(QStringLiteral(
            "the bounded active-notification capacity is full"));
    }

    const qsizetype requestPayloadBytes = retainedPayloadBytes(request);
    const qsizetype retainedWithoutPrevious =
        m_retainedPayloadBytes - previousPayloadBytes;
    const qsizetype sourceRetainedWithoutPrevious =
        usage.retainedPayloadBytes - previousPayloadBytes;
    if (requestPayloadBytes
        > m_policy.maximumRetainedPayloadBytesPerSource
            - sourceRetainedWithoutPrevious) {
        return reject(QStringLiteral(
            "the source's bounded notification payload budget is full"));
    }
    if (requestPayloadBytes
        > m_policy.maximumRetainedPayloadBytes - retainedWithoutPrevious) {
        return reject(QStringLiteral(
            "the bounded notification payload budget is full"));
    }

    SubmissionCapacityEvaluation evaluation;
    evaluation.requestPayloadBytes = requestPayloadBytes;
    return evaluation;
}

void NotificationCapacityLedger::commitSubmission(
    const QString &sourceService,
    bool replacing,
    qsizetype previousPayloadBytes,
    qsizetype requestPayloadBytes)
{
    SourceUsage &usage = m_sourceUsage[sourceService];
    Q_ASSERT(previousPayloadBytes >= 0);
    Q_ASSERT(requestPayloadBytes >= 0);
    Q_ASSERT(previousPayloadBytes <= usage.retainedPayloadBytes);
    Q_ASSERT(previousPayloadBytes <= m_retainedPayloadBytes);
    if (!replacing) {
        ++usage.activeCount;
        ++m_activeCount;
    } else {
        Q_ASSERT(usage.activeCount > 0);
    }
    usage.retainedPayloadBytes =
        usage.retainedPayloadBytes - previousPayloadBytes + requestPayloadBytes;
    m_retainedPayloadBytes =
        m_retainedPayloadBytes - previousPayloadBytes + requestPayloadBytes;
}

void NotificationCapacityLedger::release(const QString &sourceService,
                                         qsizetype payloadBytes)
{
    auto usage = m_sourceUsage.find(sourceService);
    Q_ASSERT(usage != m_sourceUsage.end());
    Q_ASSERT(usage->activeCount > 0);
    Q_ASSERT(payloadBytes >= 0);
    Q_ASSERT(payloadBytes <= usage->retainedPayloadBytes);
    Q_ASSERT(payloadBytes <= m_retainedPayloadBytes);
    --usage->activeCount;
    --m_activeCount;
    usage->retainedPayloadBytes -= payloadBytes;
    m_retainedPayloadBytes -= payloadBytes;
    if (usage->activeCount == 0) {
        Q_ASSERT(usage->retainedPayloadBytes == 0);
        m_sourceUsage.erase(usage);
    }
}

} // namespace QindaQt::Services::Notifications::Private
