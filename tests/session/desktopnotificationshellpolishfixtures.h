// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>

namespace QindaQt::Test {

[[nodiscard]] QJsonObject desktopNotificationShellTaskListFixture();
[[nodiscard]] QJsonObject desktopNotificationShellQuietingFixture();
[[nodiscard]] QJsonArray desktopNotificationShellPanelAppletsFixture();
[[nodiscard]] QJsonObject desktopNotificationShellSnapshotFixture(
    bool privatePresentationAllowed = true, bool centerOpen = false,
    bool exists = true, bool visible = false,
    QString outputName = QStringLiteral("WL-0"),
    QString centerOpenedCount = QStringLiteral("0"));
void mutateDesktopNotificationShellPolishFixture(QJsonObject &snapshot,
                                                  const QString &mutation);

} // namespace QindaQt::Test
