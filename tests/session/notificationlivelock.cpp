// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationlivelock.h"

#include "hybridtestinputdriver.h"
#include "notificationliveevidenceclient.h"
#include "notificationliveruntime.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>

namespace QindaQt::Test {
namespace {

constexpr auto FreedesktopScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto KdeScreenSaverService = "org.kde.screensaver";

std::optional<quint32> submitLockedNotification(QString *error)
{
    QDBusInterface notifications(QString::fromLatin1(NotificationLiveNotificationService),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QString::fromLatin1(NotificationLiveNotificationService));
    const QVariantMap hints{{QStringLiteral("urgency"), QVariant::fromValue(static_cast<uchar>(2))},
                            // Cleanup after unlock must not create a Recent record that could be
                            // mistaken for lock-time replay in later restart phases.
                            {QStringLiteral("transient"), true}};
    const QDBusReply<quint32> reply =
        notifications.call(QStringLiteral("Notify"), QStringLiteral("QindaQt live qualification"),
                           quint32(0), QString{}, QStringLiteral("Critical while locked"),
                           QStringLiteral("Privacy qualification"), QStringList{}, hints, 0);
    if (!reply.isValid() || reply.value() == 0) {
        *error = QStringLiteral("locked notification submission failed: %1")
                     .arg(reply.error().message());
        return std::nullopt;
    }
    return reply.value();
}

bool closeLockedNotification(quint32 id, QString *error)
{
    QDBusInterface notifications(QString::fromLatin1(NotificationLiveNotificationService),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QString::fromLatin1(NotificationLiveNotificationService));
    const QDBusMessage reply = notifications.call(QStringLiteral("CloseNotification"), id);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("locked notification close failed: %1").arg(reply.errorMessage());
        return false;
    }
    return true;
}

bool allPrivatePresentationCleared(const QJsonObject &snapshot)
{
    const QJsonObject presentation = presentationEvidence(snapshot);
    const QJsonObject popup = windowEvidence(snapshot, QLatin1StringView("popup"));
    const QJsonObject center = windowEvidence(snapshot, QLatin1StringView("center"));
    return !presentation.value(QStringLiteral("privatePresentationAllowed")).toBool()
           && !presentation.value(QStringLiteral("centerOpen")).toBool()
           && presentation.value(QStringLiteral("activeCount")).toInt(-1) == 0
           && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
           && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0
           && !presentation.value(QStringLiteral("operationBusy")).toBool()
           && presentation.value(QStringLiteral("operationErrorText")).toString().isEmpty()
           && !popup.value(QStringLiteral("visible")).toBool()
           && !center.value(QStringLiteral("visible")).toBool();
}

bool unlockedWithoutReplay(const QJsonObject &snapshot)
{
    const QJsonObject presentation = presentationEvidence(snapshot);
    return presentation.value(QStringLiteral("privatePresentationAllowed")).toBool()
           && !presentation.value(QStringLiteral("centerOpen")).toBool()
           && presentation.value(QStringLiteral("activeCount")).toInt(-1) == 1
           && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
           && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0
           && !presentation.value(QStringLiteral("operationBusy")).toBool()
           && presentation.value(QStringLiteral("operationErrorText")).toString().isEmpty()
           && !windowEvidence(snapshot, QLatin1StringView("popup"))
                   .value(QStringLiteral("visible"))
                   .toBool()
           && !windowEvidence(snapshot, QLatin1StringView("center"))
                   .value(QStringLiteral("visible"))
                   .toBool();
}

} // namespace

std::optional<QJsonObject>
collectNestedLockEvidence(const NotificationLiveExpectations &expectations,
                          DevelopmentInputDriver &input, NotificationLiveEvidenceClient &shell,
                          QString *error)
{
    QString ownerError;
    const auto compositorOwner = notificationLiveServiceOwner(
        QString::fromLatin1(NotificationLiveCompositorService), &ownerError);
    const auto freedesktopOwner = notificationLiveServiceOwner(
        QString::fromLatin1(FreedesktopScreenSaverService), &ownerError);
    const auto kdeOwner =
        notificationLiveServiceOwner(QString::fromLatin1(KdeScreenSaverService), &ownerError);
    const auto freedesktopPid =
        notificationLiveServicePid(QString::fromLatin1(FreedesktopScreenSaverService), &ownerError);
    const auto kdePid =
        notificationLiveServicePid(QString::fromLatin1(KdeScreenSaverService), &ownerError);
    if (!compositorOwner || !freedesktopOwner || !kdeOwner || *compositorOwner != *freedesktopOwner
        || *compositorOwner != *kdeOwner || !freedesktopPid || !kdePid
        || *freedesktopPid != expectations.compositorProcessId
        || *kdePid != expectations.compositorProcessId) {
        *error = QStringLiteral("KScreenLocker names were not owned by the exact "
                                "nested KWin PID: %1")
                     .arg(ownerError);
        return std::nullopt;
    }
    const auto before = shell.snapshot(error);
    if (!before || !presentationEvidence(*before).value(QStringLiteral("centerOpen")).toBool()) {
        *error = QStringLiteral("notification center was not open before lock");
        return std::nullopt;
    }
    const quint64 clearCountBefore =
        observationCount(*before, QLatin1StringView("privacyDeniedClearCount"));

    QDBusInterface screenSaver(*freedesktopOwner, QStringLiteral("/ScreenSaver"),
                               QStringLiteral("org.freedesktop.ScreenSaver"));
    const QDBusMessage lockReply = screenSaver.call(QStringLiteral("Lock"));
    if (lockReply.type() == QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("real nested KScreenLocker rejected Lock: %1")
                     .arg(lockReply.errorMessage());
        return std::nullopt;
    }
    const auto active = [&] {
        const QDBusReply<bool> reply = screenSaver.call(QStringLiteral("GetActive"));
        return reply.isValid() && reply.value();
    };
    if (!awaitNotificationLiveCondition(active)) {
        *error = QStringLiteral("real nested KScreenLocker did not become active");
        return std::nullopt;
    }
    const auto cleared = shell.awaitSnapshot(
        [clearCountBefore](const QJsonObject &snapshot) {
            return allPrivatePresentationCleared(snapshot)
                   && observationCount(snapshot, QLatin1StringView("privacyDeniedClearCount"))
                          > clearCountBefore;
        },
        error);
    if (!cleared) {
        return std::nullopt;
    }
    // AGENT-NOTE: With RequirePassword=false, KScreenLocker treats the first
    // development-device key as unlock activity before global-shortcut
    // dispatch. A synthetic Meta+N therefore cannot qualify the locked state;
    // it would qualify the post-unlock state instead. Prove the privacy-cleared
    // center remains closed without input, then use a locked host submission as
    // the live denial transition.
    processProbeEventsFor(250);
    const auto denied = shell.snapshot(error);
    if (!denied || !allPrivatePresentationCleared(*denied)) {
        *error = QStringLiteral("locked notification center did not remain cleared");
        return std::nullopt;
    }
    const auto lockedNotification = submitLockedNotification(error);
    processProbeEventsFor(300);
    const auto lockedSubmissionDenied =
        lockedNotification ? shell.awaitSnapshot(allPrivatePresentationCleared, error)
                           : std::optional<QJsonObject>{};
    if (!lockedSubmissionDenied || !input.pressKey(QLatin1StringView("space"), error)
        || !awaitNotificationLiveCondition([&] { return !active(); })) {
        if (error->isEmpty()) {
            *error = QStringLiteral("development-device activity did not unlock "
                                    "password-disabled private KScreenLocker");
        }
        return std::nullopt;
    }
    const auto unlocked = shell.awaitSnapshot(unlockedWithoutReplay, error);
    if (!unlocked) {
        return std::nullopt;
    }
    processProbeEventsFor(350);
    const auto stable = shell.snapshot(error);
    if (!stable || !unlockedWithoutReplay(*stable)) {
        *error = QStringLiteral("unlock baseline replayed private notification state");
        return std::nullopt;
    }
    if (!closeLockedNotification(*lockedNotification, error)
        || !shell.awaitSnapshot(
            [](const QJsonObject &snapshot) {
                const QJsonObject presentation = presentationEvidence(snapshot);
                return presentation.value(QStringLiteral("activeCount")).toInt(-1) == 0
                       && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
                       && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0;
            },
            error)) {
        return std::nullopt;
    }
    return QJsonObject{
        {QStringLiteral("uniqueOwner"), *compositorOwner},
        {QStringLiteral("processId"), QString::number(expectations.compositorProcessId)},
        {QStringLiteral("lockedNotificationId"), static_cast<qint64>(*lockedNotification)},
        {QStringLiteral("privacyClearObserved"), true},
        {QStringLiteral("lockedSubmissionDenied"), true},
        {QStringLiteral("lockedCenterRemainedCleared"), true},
        {QStringLiteral("unlockNoReplay"), true},
        {QStringLiteral("unlockedByDevelopmentDevice"), true},
        {QStringLiteral("passwordAuthenticationDisabledInPrivateRuntime"), true},
    };
}

} // namespace QindaQt::Test
