// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationlivesettingsphases.h"

#include "compositorprobeclient.h"
#include "hybridtestinputdriver.h"
#include "notificationliveevidenceclient.h"
#include "notificationlivekeyboard.h"
#include "notificationliveresident.h"
#include "notificationliveruntime.h"

#include <QJsonDocument>

#include <cerrno>
#include <csignal>
#include <cstring>

namespace QindaQt::Test {
namespace {

QJsonObject failure(const NotificationLiveExpectations &expectations, QString message,
                    QJsonObject evidence)
{
    return {{QStringLiteral("passed"), false},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("failure"), std::move(message)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

bool signalStoppedPrivateSettings(qint64 processId, int signal, QLatin1StringView action,
                                  QString *error)
{
    if (::kill(static_cast<pid_t>(processId), signal) == 0) {
        return true;
    }
    *error = QStringLiteral("could not %1 exact private Settings1 PID %2: %3")
                 .arg(action)
                 .arg(processId)
                 .arg(QString::fromLocal8Bit(std::strerror(errno)));
    return false;
}

bool authenticateLineage(const NotificationLiveExpectations &expectations,
                         NotificationLiveEvidenceClient &shell, QJsonObject *evidence,
                         QString *error)
{
    if (!awaitNotificationLiveService(QString::fromLatin1(NotificationLiveNotificationService))
        || !shell.authenticate(expectations.shellProcessId, error)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("notification host or shell evidence is absent");
        }
        return false;
    }
    const auto hostPid =
        notificationLiveServicePid(QString::fromLatin1(NotificationLiveNotificationService), error);
    if (!hostPid || *hostPid != expectations.notificationHostProcessId) {
        *error = QStringLiteral("resident notification host PID changed: %1").arg(*error);
        return false;
    }
    evidence->insert(QStringLiteral("notificationHostPid"), QString::number(*hostPid));
    evidence->insert(QStringLiteral("shellPid"), QString::number(expectations.shellProcessId));
    return true;
}

bool closeCenter(DevelopmentInputDriver &input, NotificationLiveEvidenceClient &shell,
                 QString *error)
{
    if (!input.pressKey(QLatin1StringView("escape"), error)) {
        return false;
    }
    return shell
        .awaitSnapshot(
            [](const QJsonObject &snapshot) {
                return !presentationEvidence(snapshot).value(QStringLiteral("centerOpen")).toBool()
                       && !windowEvidence(snapshot, QLatin1StringView("center"))
                               .value(QStringLiteral("visible"))
                               .toBool();
            },
            error)
        .has_value();
}

QJsonObject runRejected(const NotificationLiveExpectations &expectations, QJsonObject evidence,
                        NotificationLiveEvidenceClient &shell)
{
    QString error;
    CompositorProbeClient compositor;
    DevelopmentInputDriver input(compositor);
    const auto before = shell.snapshot(&error);
    if (!before || !quietingEvidence(*before).value(QStringLiteral("enabled")).toBool()
        || !openNotificationCenter(input, shell, &error)
        || !focusNotificationControl(QLatin1StringView("notificationDoNotDisturbButton"), input,
                                     shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const quint64 errorVisibleBefore =
        observationCount(*before, QLatin1StringView("quietingErrorVisibleCount"));
    if (!input.pressKey(QLatin1StringView("space"), &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto rejected = shell.awaitSnapshot(
        [&](const QJsonObject &snapshot) {
            const QJsonObject quieting = quietingEvidence(snapshot);
            const QJsonObject status = windowEvidence(snapshot, QLatin1StringView("center"))
                                           .value(QStringLiteral("quietingStatus"))
                                           .toObject();
            return quieting.value(QStringLiteral("state")) == QStringLiteral("ready")
                   && quieting.value(QStringLiteral("enabled")).toBool()
                   && !quieting.value(QStringLiteral("errorText")).toString().isEmpty()
                   && status.value(QStringLiteral("visible")).toBool()
                   && observationCount(snapshot, QLatin1StringView("quietingErrorVisibleCount"))
                          > errorVisibleBefore;
        },
        &error);
    if (!rejected || !closeCenter(input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("confirmedRejectionVisible"), true);
    evidence.insert(QStringLiteral("confirmedValuePreserved"), true);
    return {{QStringLiteral("passed"), true},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

QJsonObject runUncertain(const NotificationLiveExpectations &expectations, QJsonObject evidence,
                         NotificationLiveEvidenceClient &shell)
{
    QString error;
    const auto settingsPid =
        notificationLiveServicePid(QString::fromLatin1(NotificationLiveSettingsService), &error);
    if (!settingsPid || *settingsPid != expectations.settingsProcessId) {
        return failure(expectations,
                       QStringLiteral("Settings1 PID mismatch before uncertainty: %1").arg(error),
                       std::move(evidence));
    }
    CompositorProbeClient compositor;
    DevelopmentInputDriver input(compositor);
    if (!openNotificationCenter(input, shell, &error)
        || !focusNotificationControl(QLatin1StringView("notificationDoNotDisturbButton"), input,
                                     shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto currentSettingsPid =
        notificationLiveServicePid(QString::fromLatin1(NotificationLiveSettingsService), &error);
    if (!currentSettingsPid || *currentSettingsPid != *settingsPid
        || !validateNotificationLiveSignalTarget(*currentSettingsPid, &error)) {
        return failure(
            expectations,
            QStringLiteral("Settings1 identity changed before uncertainty signal: %1").arg(error),
            std::move(evidence));
    }
    if (!signalStoppedPrivateSettings(*currentSettingsPid, SIGSTOP, QLatin1StringView("stop"),
                                      &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    if (!input.pressKey(QLatin1StringView("space"), &error)) {
        QString resumeError;
        if (!signalStoppedPrivateSettings(*currentSettingsPid, SIGCONT, QLatin1StringView("resume"),
                                          &resumeError)) {
            error += QStringLiteral("; %1").arg(resumeError);
        }
        return failure(expectations, error, std::move(evidence));
    }
    const auto saving = shell.awaitSnapshot(
        [](const QJsonObject &snapshot) {
            const QJsonObject quieting = quietingEvidence(snapshot);
            const QJsonObject status = windowEvidence(snapshot, QLatin1StringView("center"))
                                           .value(QStringLiteral("quietingStatus"))
                                           .toObject();
            return quieting.value(QStringLiteral("state")) == QStringLiteral("saving")
                   && status.value(QStringLiteral("visible")).toBool()
                   && status.value(QStringLiteral("text")).toString() == QStringLiteral("Saving…");
        },
        &error);
    if (!saving) {
        QString resumeError;
        if (!signalStoppedPrivateSettings(*currentSettingsPid, SIGCONT, QLatin1StringView("resume"),
                                          &resumeError)) {
            error += QStringLiteral("; %1").arg(resumeError);
        }
        return failure(expectations, error, std::move(evidence));
    }
    if (!signalStoppedPrivateSettings(*currentSettingsPid, SIGKILL, QLatin1StringView("terminate"),
                                      &error)) {
        QString resumeError;
        if (!signalStoppedPrivateSettings(*currentSettingsPid, SIGCONT, QLatin1StringView("resume"),
                                          &resumeError)) {
            error += QStringLiteral("; %1").arg(resumeError);
        }
        return failure(expectations, error, std::move(evidence));
    }
    const auto uncertain = shell.awaitSnapshot(
        [](const QJsonObject &snapshot) {
            const QJsonObject quieting = quietingEvidence(snapshot);
            const QJsonObject status = windowEvidence(snapshot, QLatin1StringView("center"))
                                           .value(QStringLiteral("quietingStatus"))
                                           .toObject();
            return quieting.value(QStringLiteral("state")) == QStringLiteral("unavailable")
                   && quieting.value(QStringLiteral("hasBaseline")).toBool()
                   && quieting.value(QStringLiteral("enabled")).toBool()
                   && !quieting.value(QStringLiteral("canToggle")).toBool()
                   && quieting.value(QStringLiteral("statusText"))
                          .toString()
                          .startsWith(QStringLiteral("Last confirmed:"))
                   && !quieting.value(QStringLiteral("errorText")).toString().isEmpty()
                   && status.value(QStringLiteral("visible")).toBool();
        },
        &error);
    if (!uncertain) {
        return failure(expectations, error, std::move(evidence));
    }
    // AGENT-GUARD: Every independently launched settings phase must leave the
    // center closed. The next probe uses Meta+N as an opening assertion; carrying
    // an open center across the process boundary turns that chord into a close
    // and produces a false outage failure.
    if (!closeCenter(input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("savingVisible"), true);
    evidence.insert(QStringLiteral("uncertainVisible"), true);
    evidence.insert(QStringLiteral("lastConfirmedRetained"), true);
    evidence.insert(QStringLiteral("settingsTerminatedPid"), QString::number(*currentSettingsPid));
    return {{QStringLiteral("passed"), true},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

QJsonObject runOutage(const NotificationLiveExpectations &expectations, QJsonObject evidence,
                      NotificationLiveEvidenceClient &shell)
{
    QString ignored;
    if (notificationLiveServiceOwner(QString::fromLatin1(NotificationLiveSettingsService),
                                     &ignored)) {
        return failure(expectations, QStringLiteral("Settings1 unexpectedly owned during outage"),
                       std::move(evidence));
    }
    QString error;
    CompositorProbeClient compositor;
    DevelopmentInputDriver input(compositor);
    if (!openNotificationCenter(input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto outage = shell.awaitSnapshot(
        [](const QJsonObject &snapshot) {
            const QJsonObject quieting = quietingEvidence(snapshot);
            const QJsonObject status = windowEvidence(snapshot, QLatin1StringView("center"))
                                           .value(QStringLiteral("quietingStatus"))
                                           .toObject();
            return quieting.value(QStringLiteral("state")) == QStringLiteral("unavailable")
                   && quieting.value(QStringLiteral("hasBaseline")).toBool()
                   && quieting.value(QStringLiteral("enabled")).toBool()
                   && !quieting.value(QStringLiteral("canToggle")).toBool()
                   && quieting.value(QStringLiteral("statusText"))
                          .toString()
                          .startsWith(QStringLiteral("Last confirmed:"))
                   && status.value(QStringLiteral("visible")).toBool();
        },
        &error);
    if (!outage
        || !focusNotificationControl(QLatin1StringView("notificationSettingsRouteButton"), input,
                                     shell, &error)
        || !closeCenter(input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("failQuietLastConfirmedVisible"), true);
    evidence.insert(QStringLiteral("disabledDndSkippedByFocus"), true);
    return {{QStringLiteral("passed"), true},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

bool clearShellRestartBaseline(quint32 notificationId, DevelopmentInputDriver &input,
                               qint64 residentOwnerProcessId,
                               NotificationLiveEvidenceClient &shell, QString *error)
{
    if (!closeResidentNotification(notificationId, residentOwnerProcessId, error)
        || !shell.awaitSnapshot(
            [](const QJsonObject &snapshot) {
                const QJsonObject presentation = presentationEvidence(snapshot);
                return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 0
                       && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
                       && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 1;
            },
            error)
        || !focusNotificationControl(QLatin1StringView("notificationClearHistoryButton"), input,
                                     shell, error)
        // Qt Quick AbstractButton activation is portable through Space. Enter
        // depends on style/default-button handling and does not activate this
        // ordinary Clear history button in the staged shell.
        || !input.pressKey(QLatin1StringView("space"), error)
        || !shell.awaitSnapshot(
            [](const QJsonObject &snapshot) {
                const QJsonObject presentation = presentationEvidence(snapshot);
                return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 0
                       && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
                       && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0;
            },
            error)) {
        return false;
    }
    return closeCenter(input, shell, error);
}

QJsonObject runRecovered(const NotificationLiveExpectations &expectations, QJsonObject evidence,
                         NotificationLiveEvidenceClient &shell)
{
    QString error;
    if (!awaitNotificationLiveService(QString::fromLatin1(NotificationLiveSettingsService))) {
        return failure(expectations, QStringLiteral("restarted Settings1 did not acquire its name"),
                       std::move(evidence));
    }
    const auto settingsPid =
        notificationLiveServicePid(QString::fromLatin1(NotificationLiveSettingsService), &error);
    if (!settingsPid || *settingsPid != expectations.settingsProcessId) {
        return failure(expectations,
                       QStringLiteral("restarted Settings1 PID mismatch: %1").arg(error),
                       std::move(evidence));
    }
    const bool shellRestart = expectations.phase == NotificationLivePhase::ShellRestart;
    if (expectations.residentNotificationId == 0
        || expectations.residentOwnerProcessId <= 1) {
        return failure(expectations,
                       QStringLiteral("restart omitted resident notification owner identity"),
                       std::move(evidence));
    }
    const auto recovered = shell.awaitSnapshot(
        [shellRestart](const QJsonObject &snapshot) {
            const QJsonObject quieting = quietingEvidence(snapshot);
            const QJsonObject presentation = presentationEvidence(snapshot);
            return quieting.value(QStringLiteral("state")) == QStringLiteral("ready")
                   && quieting.value(QStringLiteral("hasBaseline")).toBool()
                   && quieting.value(QStringLiteral("enabled")).toBool()
                   && quieting.value(QStringLiteral("errorText")).toString().isEmpty()
                   && presentation.value(QStringLiteral("activeCount")).toInt(-1) == 1
                   && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
                   && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0;
        },
        &error);
    if (!recovered) {
        return failure(expectations, error, std::move(evidence));
    }
    if (!shellRestart) {
        evidence.insert(QStringLiteral("residentNotificationId"),
                        QString::number(expectations.residentNotificationId));
        evidence.insert(QStringLiteral("residentActiveWithoutPopup"), true);
    } else {
        CompositorProbeClient compositor;
        DevelopmentInputDriver input(compositor);
        if (observationCount(*recovered, QLatin1StringView("centerOpenedCount")) != 0
            || !openNotificationCenter(input, shell, &error)
            || !clearShellRestartBaseline(expectations.residentNotificationId, input,
                                          expectations.residentOwnerProcessId, shell, &error)) {
            return failure(expectations, error, std::move(evidence));
        }
        evidence.insert(QStringLiteral("residentBaselineWithoutReplay"), true);
        evidence.insert(QStringLiteral("residentClosedAndHistoryCleared"), true);
    }
    evidence.insert(QStringLiteral("settingsPid"), QString::number(*settingsPid));
    evidence.insert(QStringLiteral("dndFreshlyConfirmed"), true);
    evidence.insert(QStringLiteral("noUncertainWriteReplay"), true);
    evidence.insert(QStringLiteral("freshShellAuthentication"), shellRestart);
    return {{QStringLiteral("passed"), true},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

} // namespace

QJsonObject runNotificationLiveSettingsPhase(const NotificationLiveExpectations &expectations,
                                             QJsonObject evidence)
{
    QString error;
    NotificationLiveEvidenceClient shell;
    if (!authenticateLineage(expectations, shell, &evidence, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    switch (expectations.phase) {
    case NotificationLivePhase::SettingsRejected:
        return runRejected(expectations, std::move(evidence), shell);
    case NotificationLivePhase::SettingsUncertain:
        return runUncertain(expectations, std::move(evidence), shell);
    case NotificationLivePhase::SettingsOutage:
        return runOutage(expectations, std::move(evidence), shell);
    case NotificationLivePhase::SettingsRestart:
    case NotificationLivePhase::ShellRestart:
        return runRecovered(expectations, std::move(evidence), shell);
    case NotificationLivePhase::Primary:
        break;
    }
    return failure(expectations, QStringLiteral("invalid settings phase"), std::move(evidence));
}

} // namespace QindaQt::Test
