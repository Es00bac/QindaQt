// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationliveworkflow.h"

#include "compositorprobeclient.h"
#include "hybridtestinputdriver.h"
#include "notificationliveevidenceclient.h"
#include "notificationlivekeyboard.h"
#include "notificationlivelock.h"
#include "notificationliveruntime.h"
#include "notificationlivesettingsphases.h"
#include "notificationliveshortcut.h"
#include "notificationlivesurfaces.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Test {
namespace {

constexpr auto FreedesktopScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto KdeScreenSaverService = "org.kde.screensaver";
constexpr auto GlobalAccelService = "org.kde.kglobalaccel";
constexpr auto ShellEvidenceService = "org.qindaqt.ShellDevelopment";
constexpr auto SettingsWindowTitle = "QindaQt Settings — Notifications";

QJsonObject failure(const NotificationLiveExpectations &expectations, QString message,
                    QJsonObject evidence = {})
{
    return {{QStringLiteral("passed"), false},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("failure"), std::move(message)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

bool authenticateLineage(const NotificationLiveExpectations &expectations,
                         NotificationLiveEvidenceClient &shell, QJsonObject *evidence,
                         QString *error)
{
    for (const auto *service : {
             NotificationLiveSettingsService,
             GlobalAccelService,
             NotificationLiveNotificationService,
             FreedesktopScreenSaverService,
             KdeScreenSaverService,
             ShellEvidenceService,
         }) {
        if (!awaitNotificationLiveService(QString::fromLatin1(service))) {
            *error = QStringLiteral("required private service missing: %1")
                         .arg(QString::fromLatin1(service));
            return false;
        }
    }
    const auto hostPid =
        notificationLiveServicePid(QString::fromLatin1(NotificationLiveNotificationService), error);
    const auto settingsPid =
        notificationLiveServicePid(QString::fromLatin1(NotificationLiveSettingsService), error);
    const auto globalAccelPid =
        notificationLiveServicePid(QString::fromLatin1(GlobalAccelService), error);
    if (!hostPid || *hostPid != expectations.notificationHostProcessId || !settingsPid
        || *settingsPid != expectations.settingsProcessId || !globalAccelPid
        || *globalAccelPid != expectations.compositorProcessId
        || !shell.authenticate(expectations.shellProcessId, error)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("staged service PID lineage did not match");
        }
        return false;
    }
    evidence->insert(QStringLiteral("notificationHostPid"), QString::number(*hostPid));
    evidence->insert(QStringLiteral("settingsPid"), QString::number(*settingsPid));
    evidence->insert(QStringLiteral("globalAccelPid"), QString::number(*globalAccelPid));
    evidence->insert(QStringLiteral("shellPid"), QString::number(expectations.shellProcessId));
    return true;
}

std::optional<quint32> submitNotification(bool critical, QString summary, bool withActions,
                                          QString *error)
{
    QDBusInterface notifications(QString::fromLatin1(NotificationLiveNotificationService),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QString::fromLatin1(NotificationLiveNotificationService));
    QVariantMap hints;
    hints.insert(QStringLiteral("urgency"),
                 QVariant::fromValue(static_cast<uchar>(critical ? 2 : 1)));
    const QStringList actions =
        withActions ? QStringList{QStringLiteral("open"),   QStringLiteral("Open"),
                                  QStringLiteral("second"), QStringLiteral("Second"),
                                  QStringLiteral("third"),  QStringLiteral("Third")}
                    : QStringList{};
    const QDBusReply<quint32> reply =
        notifications.call(QStringLiteral("Notify"), QStringLiteral("QindaQt live qualification"),
                           quint32(0), QString{}, std::move(summary),
                           QStringLiteral("Keyboard and privacy qualification"), actions, hints, 0);
    if (!reply.isValid() || reply.value() == 0) {
        *error = QStringLiteral("notification submission failed: %1").arg(reply.error().message());
        return std::nullopt;
    }
    return reply.value();
}

bool closeNotification(quint32 id, QString *error)
{
    QDBusInterface notifications(QString::fromLatin1(NotificationLiveNotificationService),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QString::fromLatin1(NotificationLiveNotificationService));
    const QDBusMessage reply = notifications.call(QStringLiteral("CloseNotification"), id);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("notification close failed: %1").arg(reply.errorMessage());
        return false;
    }
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

bool toggleDoNotDisturb(bool enabled, DevelopmentInputDriver &input,
                        NotificationLiveEvidenceClient &shell, QString *error)
{
    if (!focusNotificationControl(QLatin1StringView("notificationDoNotDisturbButton"), input, shell,
                                  error)
        || !input.pressKey(QLatin1StringView("space"), error)) {
        return false;
    }
    return shell
        .awaitSnapshot(
            [enabled](const QJsonObject &snapshot) {
                const QJsonObject quieting = quietingEvidence(snapshot);
                return quieting.value(QStringLiteral("state")) == QStringLiteral("ready")
                       && quieting.value(QStringLiteral("hasBaseline")).toBool()
                       && quieting.value(QStringLiteral("enabled")).toBool() == enabled;
            },
            error)
        .has_value();
}

bool assertNoCenterDispatch(NotificationLiveEvidenceClient &shell, quint64 openedBefore,
                            QString *error)
{
    processProbeEventsFor(300);
    const auto snapshot = shell.snapshot(error);
    return snapshot && !presentationEvidence(*snapshot).value(QStringLiteral("centerOpen")).toBool()
           && observationCount(*snapshot, QLatin1StringView("centerOpenedCount")) == openedBefore;
}

class ShortcutRestore final {
public:
    explicit ShortcutRestore(const NotificationLiveShortcut &shortcut) : m_shortcut(shortcut) {}
    ~ShortcutRestore()
    {
        if (m_armed) {
            QString ignored;
            static_cast<void>(
                setNotificationLiveShortcut(m_shortcut, m_shortcut.defaultKeys, &ignored));
        }
    }
    void arm() noexcept { m_armed = true; }
    void disarm() noexcept { m_armed = false; }

private:
    const NotificationLiveShortcut &m_shortcut;
    bool m_armed = false;
};

bool exerciseShortcutRemapping(const NotificationLiveShortcut &shortcut,
                               DevelopmentInputDriver &input, NotificationLiveEvidenceClient &shell,
                               QJsonObject *evidence, QString *error)
{
    ShortcutRestore restore(shortcut);
    restore.arm();
    if (!setNotificationLiveShortcut(shortcut, {}, error)) {
        return false;
    }
    const auto beforeDisabled = shell.snapshot(error);
    const quint64 openedBefore =
        beforeDisabled ? observationCount(*beforeDisabled, QLatin1StringView("centerOpenedCount"))
                       : 0;
    if (!beforeDisabled
        || !input.pressChord({QLatin1StringView("left-meta"), QLatin1StringView("n")}, error)
        || !assertNoCenterDispatch(shell, openedBefore, error)
        || !setNotificationLiveShortcut(shortcut, notificationLiveMetaShiftN(), error)
        || !input.pressChord({QLatin1StringView("left-meta"), QLatin1StringView("n")}, error)
        || !assertNoCenterDispatch(shell, openedBefore, error)
        || !input.pressChord({QLatin1StringView("left-meta"), QLatin1StringView("left-shift"),
                              QLatin1StringView("n")},
                             error)
        || !shell.awaitSnapshot(
            [openedBefore](const QJsonObject &snapshot) {
                return presentationEvidence(snapshot).value(QStringLiteral("centerOpen")).toBool()
                       && observationCount(snapshot, QLatin1StringView("centerOpenedCount"))
                              == openedBefore + 1;
            },
            error)
        || !closeCenter(input, shell, error)
        || !setNotificationLiveShortcut(shortcut, shortcut.defaultKeys, error)) {
        return false;
    }
    restore.disarm();
    if (!openNotificationCenter(input, shell, error) || !closeCenter(input, shell, error)) {
        return false;
    }
    evidence->insert(QStringLiteral("disabledBindingNonDispatch"), true);
    evidence->insert(QStringLiteral("oldBindingNonDispatchAfterRemap"), true);
    evidence->insert(QStringLiteral("remappedBindingDispatched"), true);
    evidence->insert(QStringLiteral("defaultBindingRestored"), true);
    return true;
}

bool prepareNotificationLockPhase(quint32 firstNormal, quint32 critical,
                                  DevelopmentInputDriver &input,
                                  NotificationLiveEvidenceClient &shell, QString *error)
{
    return closeNotification(firstNormal, error) && closeNotification(critical, error)
           && shell.awaitSnapshot(
               [](const QJsonObject &snapshot) {
                   return presentationEvidence(snapshot)
                                  .value(QStringLiteral("activeCount"))
                                  .toInt(-1)
                              == 0
                          && presentationEvidence(snapshot)
                                     .value(QStringLiteral("historyCount"))
                                     .toInt(-1)
                                 >= 3;
               },
               error)
           && openNotificationCenter(input, shell, error);
}

bool doNotDisturbRemainsConfirmed(NotificationLiveEvidenceClient &shell, QString *error)
{
    const auto snapshot = shell.snapshot(error);
    if (snapshot) {
        const QJsonObject quieting = quietingEvidence(*snapshot);
        if (quieting.value(QStringLiteral("state")) == QStringLiteral("ready")
            && quieting.value(QStringLiteral("hasBaseline")).toBool()
            && quieting.value(QStringLiteral("enabled")).toBool()) {
            return true;
        }
    }
    if (error->isEmpty()) {
        *error = QStringLiteral("Do Not Disturb was not confirmed after lock qualification");
    }
    return false;
}

QJsonObject runPrimary(const NotificationLiveExpectations &expectations, QJsonObject evidence)
{
    QString error;
    NotificationLiveEvidenceClient shell;
    if (!authenticateLineage(expectations, shell, &evidence, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto initial = shell.awaitSnapshot(
        [](const QJsonObject &snapshot) {
            const QJsonObject presentation = presentationEvidence(snapshot);
            const QJsonObject quieting = quietingEvidence(snapshot);
            return presentation.value(QStringLiteral("privatePresentationAllowed")).toBool()
                   && presentation.value(QStringLiteral("activeCount")).toInt(-1) == 0
                   && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
                   && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0
                   && quieting.value(QStringLiteral("state")) == QStringLiteral("ready")
                   && quieting.value(QStringLiteral("hasBaseline")).toBool()
                   && !quieting.value(QStringLiteral("enabled")).toBool();
        },
        &error);
    const auto shortcut = discoverNotificationLiveShortcut(&error);
    if (!initial || !shortcut) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("globalAccelComponent"), shortcut->actionId.constFirst());
    evidence.insert(QStringLiteral("globalAccelDefaultKeys"), shortcut->defaultKeys.size());

    CompositorProbeClient compositor;
    DevelopmentInputDriver input(compositor);
    const auto firstNormal =
        submitNotification(false, QStringLiteral("Normal before Do Not Disturb"), true, &error);
    const auto popup =
        firstNormal
            ? shell.awaitSnapshot(
                  [](const QJsonObject &snapshot) {
                      const QJsonObject presentation = presentationEvidence(snapshot);
                      return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 1
                             && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 1
                             && windowEvidence(snapshot, QLatin1StringView("popup"))
                                    .value(QStringLiteral("visible"))
                                    .toBool();
                  },
                  &error)
            : std::optional<QJsonObject>{};
    if (!popup
        || !validateNotificationLiveSurface(QLatin1StringView("notification-popup"), *popup,
                                            expectations, compositor, &error)
        || !openNotificationCenter(input, shell, &error, true)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("initialFocusCloseButton"), true);
    const auto center = shell.snapshot(&error);
    if (!center
        || !validateNotificationLiveSurface(QLatin1StringView("notification-center"), *center,
                                            expectations, compositor, &error)
        || !toggleDoNotDisturb(true, input, shell, &error) || !closeCenter(input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("keyboardDndEnabled"), true);

    const auto suppressedNormal = submitNotification(
        false, QStringLiteral("Normal while Do Not Disturb is on"), false, &error);
    const auto suppressed =
        suppressedNormal
            ? shell.awaitSnapshot(
                  [](const QJsonObject &snapshot) {
                      const QJsonObject presentation = presentationEvidence(snapshot);
                      return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 2
                             && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
                             && !windowEvidence(snapshot, QLatin1StringView("popup"))
                                     .value(QStringLiteral("visible"))
                                     .toBool();
                  },
                  &error)
            : std::optional<QJsonObject>{};
    const auto critical = submitNotification(
        true, QStringLiteral("Critical bypass while Do Not Disturb is on"), false, &error);
    const auto criticalPopup =
        critical ? shell.awaitSnapshot(
                       [](const QJsonObject &snapshot) {
                           const QJsonObject presentation = presentationEvidence(snapshot);
                           return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 3
                                  && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 1
                                  && windowEvidence(snapshot, QLatin1StringView("popup"))
                                         .value(QStringLiteral("visible"))
                                         .toBool();
                       },
                       &error)
                 : std::optional<QJsonObject>{};
    if (!suppressed || !criticalPopup
        || !validateNotificationLiveSurface(QLatin1StringView("notification-popup"), *criticalPopup,
                                            expectations, compositor, &error)
        || !closeNotification(*suppressedNormal, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto retained = shell.awaitSnapshot(
        [](const QJsonObject &snapshot) {
            const QJsonObject presentation = presentationEvidence(snapshot);
            return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 2
                   && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 1
                   && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 1;
        },
        &error);
    if (!retained || !openNotificationCenter(input, shell, &error)
        || !exerciseCompleteNotificationFocusTraversal(input, shell, &error)
        || !toggleDoNotDisturb(false, input, shell, &error) || !closeCenter(input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("completeForwardReverseFocus"), true);
    processProbeEventsFor(350);
    const auto noReplay = shell.snapshot(&error);
    if (!noReplay
        || presentationEvidence(*noReplay).value(QStringLiteral("popupCount")).toInt(-1) != 0
        || presentationEvidence(*noReplay).value(QStringLiteral("historyCount")).toInt(-1) != 1
        || !openNotificationCenter(input, shell, &error)
        || !toggleDoNotDisturb(true, input, shell, &error)
        || !focusNotificationControl(QLatin1StringView("notificationSettingsRouteButton"), input,
                                     shell, &error)
        // Qt Quick AbstractButton's portable keyboard activation is Space;
        // Enter behavior varies with platform/default-button integration.
        || !input.pressKey(QLatin1StringView("space"), &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto settingsWindow = compositor.awaitWindows(
        // The expected title contains an em dash. Decode the source literal as
        // UTF-8; fromLatin1 silently produces a different three-codepoint key.
        {QString::fromUtf8(SettingsWindowTitle)},
        [](const WindowInventory &windows) {
            // awaitWindows has already parsed every geometry field and
            // selected exactly the requested title before invoking this
            // predicate. Presence is the settings-route behavior under test.
            return !windows.isEmpty();
        },
        &error, 7'500);
    if (!settingsWindow
        || !input.pressChord({QLatin1StringView("left-meta"), QLatin1StringView("n")}, &error)
        || !shell.awaitSnapshot(
            [](const QJsonObject &snapshot) {
                return !presentationEvidence(snapshot).value(QStringLiteral("centerOpen")).toBool();
            },
            &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("normalSuppressedRetained"), true);
    evidence.insert(QStringLiteral("criticalBypassVisible"), true);
    evidence.insert(QStringLiteral("suppressedPopupNotReplayed"), true);
    evidence.insert(QStringLiteral("keyboardSettingsAction"), true);

    if (!exerciseShortcutRemapping(*shortcut, input, shell, &evidence, &error)) {
        return failure(expectations, error, std::move(evidence));
    }

    if (!prepareNotificationLockPhase(*firstNormal, *critical, input, shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    const auto locker = collectNestedLockEvidence(expectations, input, shell, &error);
    if (!locker || !doNotDisturbRemainsConfirmed(shell, &error)) {
        return failure(expectations, error, std::move(evidence));
    }
    evidence.insert(QStringLiteral("lock"), *locker);
    evidence.insert(QStringLiteral("developmentInputDeviceId"), input.deviceId());
    evidence.insert(QStringLiteral("developmentInputRequestCount"), input.requestCount());
    evidence.insert(QStringLiteral("dndConfirmedAtEnd"), true);
    return {{QStringLiteral("passed"), true},
            {QStringLiteral("phase"), notificationLivePhaseName(expectations.phase)},
            {QStringLiteral("evidence"), std::move(evidence)}};
}

} // namespace

QString notificationLivePhaseName(NotificationLivePhase phase)
{
    switch (phase) {
    case NotificationLivePhase::Primary:
        return QStringLiteral("primary");
    case NotificationLivePhase::SettingsRejected:
        return QStringLiteral("settings-rejected");
    case NotificationLivePhase::SettingsUncertain:
        return QStringLiteral("settings-uncertain");
    case NotificationLivePhase::SettingsOutage:
        return QStringLiteral("settings-outage");
    case NotificationLivePhase::SettingsRestart:
        return QStringLiteral("settings-restart");
    case NotificationLivePhase::ShellRestart:
        return QStringLiteral("shell-restart");
    }
    return QStringLiteral("unknown");
}

QJsonObject runNotificationLiveWorkflow(const NotificationLiveExpectations &expectations)
{
    QString error;
    auto runtime = validateNotificationLiveRuntime(expectations, &error);
    if (!runtime) {
        return failure(expectations, error);
    }
    if (expectations.phase == NotificationLivePhase::Primary) {
        return runPrimary(expectations, std::move(*runtime));
    }
    return runNotificationLiveSettingsPhase(expectations, std::move(*runtime));
}

} // namespace QindaQt::Test
