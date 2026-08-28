// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationliveworkflow.h"
#include "notificationliveresident.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>

#include <optional>

namespace {

std::optional<QindaQt::Test::NotificationLivePhase> phaseFromName(
    const QString &name)
{
    using QindaQt::Test::NotificationLivePhase;
    if (name == QLatin1String("primary")) {
        return NotificationLivePhase::Primary;
    }
    if (name == QLatin1String("settings-rejected")) {
        return NotificationLivePhase::SettingsRejected;
    }
    if (name == QLatin1String("settings-uncertain")) {
        return NotificationLivePhase::SettingsUncertain;
    }
    if (name == QLatin1String("settings-outage")) {
        return NotificationLivePhase::SettingsOutage;
    }
    if (name == QLatin1String("settings-restart")) {
        return NotificationLivePhase::SettingsRestart;
    }
    if (name == QLatin1String("shell-restart")) {
        return NotificationLivePhase::ShellRestart;
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("qindaqt-notification-live-probe"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {QStringLiteral("resident-owner"),
         QStringLiteral("Run the private resident notification owner")},
        {QStringLiteral("phase"), QStringLiteral("Workflow phase"),
         QStringLiteral("name")},
        {QStringLiteral("compositor-pid"), QStringLiteral("Expected KWin PID"),
         QStringLiteral("pid")},
        {QStringLiteral("notification-host-pid"),
         QStringLiteral("Expected resident notification host PID"),
         QStringLiteral("pid")},
        {QStringLiteral("settings-pid"), QStringLiteral("Expected Settings1 PID"),
         QStringLiteral("pid"), QStringLiteral("0")},
        {QStringLiteral("shell-pid"), QStringLiteral("Expected shell PID"),
         QStringLiteral("pid")},
        {QStringLiteral("resident-notification-id"),
         QStringLiteral("Host-resident record expected after shell restart"),
         QStringLiteral("id"), QStringLiteral("0")},
        {QStringLiteral("resident-owner-pid"),
         QStringLiteral("Expected private resident sender PID"),
         QStringLiteral("pid"), QStringLiteral("0")},
        {QStringLiteral("logical-width"), QStringLiteral("Expected logical width"),
         QStringLiteral("pixels")},
        {QStringLiteral("logical-height"), QStringLiteral("Expected logical height"),
         QStringLiteral("pixels")},
        {QStringLiteral("scale"), QStringLiteral("Expected output scale"),
         QStringLiteral("factor")},
    });
    parser.process(application);
    if (parser.isSet(QStringLiteral("resident-owner"))) {
        return QindaQt::Test::runResidentNotificationOwner();
    }

    bool compositorPidValid = false;
    bool notificationHostPidValid = false;
    bool settingsPidValid = false;
    bool shellPidValid = false;
    bool residentNotificationIdValid = false;
    bool residentOwnerPidValid = false;
    bool widthValid = false;
    bool heightValid = false;
    bool scaleValid = false;
    const auto phase = phaseFromName(parser.value(QStringLiteral("phase")));
    QindaQt::Test::NotificationLiveExpectations expectations;
    expectations.phase = phase.value_or(
        QindaQt::Test::NotificationLivePhase::Primary);
    expectations.compositorProcessId = parser.value(
        QStringLiteral("compositor-pid")).toLongLong(&compositorPidValid);
    expectations.notificationHostProcessId = parser.value(
        QStringLiteral("notification-host-pid"))
        .toLongLong(&notificationHostPidValid);
    expectations.settingsProcessId = parser.value(
        QStringLiteral("settings-pid")).toLongLong(&settingsPidValid);
    expectations.shellProcessId = parser.value(
        QStringLiteral("shell-pid")).toLongLong(&shellPidValid);
    expectations.residentNotificationId = parser.value(
        QStringLiteral("resident-notification-id"))
        .toUInt(&residentNotificationIdValid);
    expectations.residentOwnerProcessId = parser.value(
        QStringLiteral("resident-owner-pid")).toLongLong(&residentOwnerPidValid);
    expectations.logicalWidth = parser.value(
        QStringLiteral("logical-width")).toInt(&widthValid);
    expectations.logicalHeight = parser.value(
        QStringLiteral("logical-height")).toInt(&heightValid);
    expectations.scale = parser.value(QStringLiteral("scale")).toDouble(
        &scaleValid);
    if (!phase || !compositorPidValid || expectations.compositorProcessId <= 1
        || !settingsPidValid || expectations.settingsProcessId < 0
        || !notificationHostPidValid
        || expectations.notificationHostProcessId <= 1 || !shellPidValid
        || expectations.shellProcessId <= 1
        || !residentNotificationIdValid
        || !residentOwnerPidValid || expectations.residentOwnerProcessId < 0
        || !widthValid || expectations.logicalWidth <= 0 || !heightValid
        || expectations.logicalHeight <= 0 || !scaleValid
        || expectations.scale <= 0.0) {
        QTextStream(stderr) << "invalid notification-live probe arguments\n";
        return 2;
    }

    const QJsonObject result =
        QindaQt::Test::runNotificationLiveWorkflow(expectations);
    QTextStream(stdout)
        << "QINDAQT_NOTIFICATION_LIVE="
        << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    return result.value(QStringLiteral("passed")).toBool() ? 0 : 1;
}
