// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_types.h"

#include <QHash>

namespace QindaQt::Services::Notifications::Private {

struct SubmissionCapacityEvaluation final {
    qsizetype requestPayloadBytes = 0;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

// Constant-time usage-accounting ledger for global and authenticated sources.
// The owning NotificationService must commit exactly once after each accepted
// model mutation and release exactly once for every removed entry.
class NotificationCapacityLedger final {
public:
    explicit NotificationCapacityLedger(NotificationPolicy policy);

    // AGENT-CONTRACT: The caller must reject replacement ownership mismatches
    // before evaluation. That makes previousPayloadBytes part of the same
    // authenticated source tracked by this ledger.
    [[nodiscard]] SubmissionCapacityEvaluation evaluateSubmission(
        const NotificationRequest &request,
        bool replacing,
        qsizetype previousPayloadBytes) const;

    void commitSubmission(const QString &sourceService,
                          bool replacing,
                          qsizetype previousPayloadBytes,
                          qsizetype requestPayloadBytes);
    void release(const QString &sourceService, qsizetype payloadBytes);

private:
    struct SourceUsage final {
        qsizetype activeCount = 0;
        qsizetype retainedPayloadBytes = 0;
    };

    NotificationPolicy m_policy;
    QHash<QString, SourceUsage> m_sourceUsage;
    qsizetype m_activeCount = 0;
    qsizetype m_retainedPayloadBytes = 0;
};

} // namespace QindaQt::Services::Notifications::Private
