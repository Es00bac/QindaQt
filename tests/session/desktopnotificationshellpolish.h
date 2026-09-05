// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>

namespace QindaQt::Test {

struct DesktopNotificationShellObservation;

enum class DesktopNotificationShellPolishReadiness {
    Ready,
    TaskListPending,
    QuietingPending,
    InvalidTaskList,
    InvalidPanelApplets,
};

[[nodiscard]] DesktopNotificationShellPolishReadiness
desktopNotificationShellPolishReadiness(
    const QJsonObject &taskList, const QJsonObject &quieting,
    const QJsonArray &panelApplets);

[[nodiscard]] QJsonObject desktopNotificationShellNormalizedEvidence(
    const DesktopNotificationShellObservation &observation,
    qint64 shellProcessId, bool privatePresentationAllowed, bool centerOpen,
    quint64 centerOpenedCount, const QJsonObject &center,
    const QJsonObject &tokens, const QJsonObject &taskList,
    const QJsonObject &quieting, const QJsonArray &panelApplets);

} // namespace QindaQt::Test
