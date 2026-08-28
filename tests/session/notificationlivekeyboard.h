// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Test {

class DevelopmentInputDriver;
class NotificationLiveEvidenceClient;

[[nodiscard]] bool openNotificationCenter(
    DevelopmentInputDriver &input, NotificationLiveEvidenceClient &evidence,
    QString *error, bool requireInitialCloseFocus = false);
[[nodiscard]] bool focusNotificationControl(
    QLatin1StringView objectName, DevelopmentInputDriver &input,
    NotificationLiveEvidenceClient &evidence, QString *error);
[[nodiscard]] bool exerciseCompleteNotificationFocusTraversal(
    DevelopmentInputDriver &input, NotificationLiveEvidenceClient &evidence,
    QString *error);

} // namespace QindaQt::Test
