// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class QDBusInterface;

namespace QindaQt::Test {

class NotificationLiveEvidenceClient final {
public:
    NotificationLiveEvidenceClient();
    ~NotificationLiveEvidenceClient();

    NotificationLiveEvidenceClient(const NotificationLiveEvidenceClient &) = delete;
    NotificationLiveEvidenceClient &operator=(const NotificationLiveEvidenceClient &) = delete;

    [[nodiscard]] bool authenticate(qint64 expectedShellProcessId, QString *error);
    [[nodiscard]] std::optional<QJsonObject> snapshot(QString *error) const;
    [[nodiscard]] std::optional<QJsonObject>
    awaitSnapshot(const std::function<bool(const QJsonObject &)> &condition, QString *error,
                  int timeoutMilliseconds = 7'500) const;

private:
    std::unique_ptr<QDBusInterface> m_endpoint;
    qint64 m_authenticatedShellProcessId = 0;
};

[[nodiscard]] QJsonObject presentationEvidence(const QJsonObject &snapshot);
[[nodiscard]] QJsonObject quietingEvidence(const QJsonObject &snapshot);
[[nodiscard]] QJsonObject windowEvidence(const QJsonObject &snapshot, QLatin1StringView role);
[[nodiscard]] quint64 observationCount(const QJsonObject &snapshot, QLatin1StringView name);

} // namespace QindaQt::Test
