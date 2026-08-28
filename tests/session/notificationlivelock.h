// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "notificationliveworkflow.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace QindaQt::Test {

class DevelopmentInputDriver;
class NotificationLiveEvidenceClient;

[[nodiscard]] std::optional<QJsonObject> collectNestedLockEvidence(
    const NotificationLiveExpectations &expectations,
    DevelopmentInputDriver &input, NotificationLiveEvidenceClient &shell,
    QString *error);

} // namespace QindaQt::Test
