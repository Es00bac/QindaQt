// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationshellpolish.h"
#include "desktopnotificationshellreadiness.h"

#include <QHash>
#include <QJsonValue>

namespace QindaQt::Test {
namespace {

enum class TaskListReadiness { Ready, Pending, Invalid };

bool canonicalCounter(const QJsonValue &value, quint64 *result)
{
    if (!value.isString()) {
        return false;
    }
    bool converted = false;
    const quint64 parsed = value.toString().toULongLong(&converted);
    if (!converted || value.toString() != QString::number(parsed)) {
        return false;
    }
    *result = parsed;
    return true;
}

TaskListReadiness taskListReadiness(const QJsonObject &taskList)
{
    quint64 generation = 0;
    const QJsonValue count = taskList.value(QStringLiteral("windowCount"));
    const QJsonValue phaseValue = taskList.value(QStringLiteral("phase"));
    const QJsonValue buttonsValue = taskList.value(QStringLiteral("buttons"));
    if (taskList.size() != 4 || !phaseValue.isString()
        || !canonicalCounter(taskList.value(QStringLiteral("generation")),
                             &generation)
        || !count.isDouble() || count.toDouble() != count.toInt()
        || count.toInt() < 0 || !buttonsValue.isArray()) {
        return TaskListReadiness::Invalid;
    }
    const QJsonArray buttons = buttonsValue.toArray();
    if (buttons.size() != count.toInt()) {
        return TaskListReadiness::Invalid;
    }
    for (const QJsonValue &value : buttons) {
        const QJsonObject button = value.toObject();
        const QString applicationId =
            button.value(QStringLiteral("applicationId")).toString();
        if (!value.isObject() || button.size() != 3 || applicationId.isEmpty()
            || applicationId == QLatin1StringView("qindaqt-shell")
            || button.value(QStringLiteral("iconName")).toString().isEmpty()
            || button.value(QStringLiteral("iconResolved")) != QJsonValue(true)) {
            return TaskListReadiness::Invalid;
        }
    }
    const QString phase = phaseValue.toString();
    if (phase != QStringLiteral("loading") && phase != QStringLiteral("ready")
        && phase != QStringLiteral("empty")
        && phase != QStringLiteral("degraded")
        && phase != QStringLiteral("unavailable")) {
        return TaskListReadiness::Invalid;
    }
    return phase == QStringLiteral("ready") && generation > 0
            && count.toInt() > 0
        ? TaskListReadiness::Ready : TaskListReadiness::Pending;
}

bool quietingReady(const QJsonObject &quieting)
{
    return quieting == QJsonObject{
        {QStringLiteral("enabled"), false},
        {QStringLiteral("hasBaseline"), true},
        {QStringLiteral("state"), QStringLiteral("ready")},
        {QStringLiteral("canToggle"), true},
        {QStringLiteral("statusText"), QString{}},
        {QStringLiteral("errorText"), QString{}},
    };
}

bool panelAppletsReady(const QJsonArray &applets)
{
    QHash<QString, int> taskListsByPanel;
    for (const QJsonValue &value : applets) {
        const QJsonObject applet = value.toObject();
        if (!value.isObject() || applet.size() != 5) {
            return false;
        }
        const QString plugin = applet.value(QStringLiteral("plugin")).toString();
        if (plugin == QLatin1StringView("application-launcher")
            || plugin == QLatin1StringView("grouped-task-list")
            || plugin == QLatin1StringView("dock-task-list")
            || plugin == QLatin1StringView("centered-task-list")) {
            return false;
        }
        if (plugin == QLatin1StringView("launcher")
            && (applet.value(QStringLiteral("ready")) != QJsonValue(true)
                || applet.value(QStringLiteral("entryPoint"))
                    != QJsonValue(QStringLiteral("qindaqt.applets.launcher")))) {
            return false;
        }
        if (plugin == QLatin1StringView("task-list")) {
            const QString panelId =
                applet.value(QStringLiteral("panelId")).toString();
            if (++taskListsByPanel[panelId] > 1) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

DesktopNotificationShellPolishReadiness
desktopNotificationShellPolishReadiness(
    const QJsonObject &taskList, const QJsonObject &quieting,
    const QJsonArray &panelApplets)
{
    switch (taskListReadiness(taskList)) {
    case TaskListReadiness::Invalid:
        return DesktopNotificationShellPolishReadiness::InvalidTaskList;
    case TaskListReadiness::Pending:
        return DesktopNotificationShellPolishReadiness::TaskListPending;
    case TaskListReadiness::Ready:
        break;
    }
    if (!quietingReady(quieting)) {
        return DesktopNotificationShellPolishReadiness::QuietingPending;
    }
    return panelAppletsReady(panelApplets)
        ? DesktopNotificationShellPolishReadiness::Ready
        : DesktopNotificationShellPolishReadiness::InvalidPanelApplets;
}

QJsonObject desktopNotificationShellNormalizedEvidence(
    const DesktopNotificationShellObservation &observation,
    qint64 shellProcessId, bool privatePresentationAllowed, bool centerOpen,
    quint64 centerOpenedCount, const QJsonObject &center,
    const QJsonObject &tokens, const QJsonObject &taskList,
    const QJsonObject &quieting, const QJsonArray &panelApplets)
{
    QJsonObject centerEvidence{{QStringLiteral("exists"),
                                center.value(QStringLiteral("exists"))}};
    if (center.value(QStringLiteral("exists")).toBool()) {
        centerEvidence.insert(QStringLiteral("visible"),
                              center.value(QStringLiteral("visible")));
        centerEvidence.insert(QStringLiteral("outputName"),
                              center.value(QStringLiteral("outputName")));
    }
    return {
        {QStringLiteral("owner"), observation.owner},
        {QStringLiteral("servicePid"),
         QString::number(observation.serviceProcessId)},
        {QStringLiteral("shellPid"), QString::number(shellProcessId)},
        {QStringLiteral("tokens"), tokens},
        {QStringLiteral("taskList"), taskList},
        {QStringLiteral("quieting"), quieting},
        {QStringLiteral("panelApplets"), panelApplets},
        {QStringLiteral("presentation"),
         QJsonObject{
             {QStringLiteral("privatePresentationAllowed"),
              privatePresentationAllowed},
             {QStringLiteral("centerOpen"), centerOpen},
         }},
        {QStringLiteral("centerOpenedCount"), QString::number(centerOpenedCount)},
        {QStringLiteral("centerWindow"), centerEvidence},
    };
}

} // namespace QindaQt::Test
