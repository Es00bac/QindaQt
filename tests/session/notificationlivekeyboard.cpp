// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationlivekeyboard.h"

#include "hybridtestinputdriver.h"
#include "notificationliveevidenceclient.h"

#include <QJsonArray>

#include <algorithm>

namespace QindaQt::Test {
namespace {

QStringList names(const QJsonValue &value)
{
    QStringList result;
    for (const QJsonValue &entry : value.toArray()) {
        if (entry.isString() && !entry.toString().isEmpty()) {
            result.append(entry.toString());
        }
    }
    return result;
}

bool awaitFocus(NotificationLiveEvidenceClient &evidence,
                const QString &expected, QString *error)
{
    return evidence.awaitSnapshot(
        [&](const QJsonObject &snapshot) {
            return windowEvidence(snapshot, QLatin1StringView("center"))
                       .value(QStringLiteral("activeFocusItem"))
                == expected;
        }, error)
        .has_value();
}

bool injectFocusSequence(const QStringList &sequence, bool reverse,
                         DevelopmentInputDriver &input,
                         NotificationLiveEvidenceClient &evidence,
                         QString *error)
{
    if (sequence.size() < 2) {
        *error = QStringLiteral("notification center focus chain is incomplete");
        return false;
    }
    for (qsizetype index = 1; index < sequence.size(); ++index) {
        const bool injected = reverse
            ? input.pressChord({QLatin1StringView("left-shift"),
                                QLatin1StringView("tab")}, error)
            : input.pressKey(QLatin1StringView("tab"), error);
        if (!injected || !awaitFocus(evidence, sequence.at(index), error)) {
            return false;
        }
    }
    const bool wrapped = reverse
        ? input.pressChord({QLatin1StringView("left-shift"),
                            QLatin1StringView("tab")}, error)
        : input.pressKey(QLatin1StringView("tab"), error);
    return wrapped && awaitFocus(evidence, sequence.constFirst(), error);
}

} // namespace

bool openNotificationCenter(DevelopmentInputDriver &input,
                            NotificationLiveEvidenceClient &evidence,
                            QString *error, bool requireInitialCloseFocus)
{
    const auto before = evidence.snapshot(error);
    if (!before) {
        return false;
    }
    const quint64 openedBefore =
        observationCount(*before, QLatin1StringView("centerOpenedCount"));
    if (!input.pressChord({QLatin1StringView("left-meta"),
                           QLatin1StringView("n")}, error)) {
        return false;
    }
    return evidence.awaitSnapshot(
        [&](const QJsonObject &snapshot) {
            const QJsonObject presentation = presentationEvidence(snapshot);
            const QJsonObject center =
                windowEvidence(snapshot, QLatin1StringView("center"));
            return presentation.value(QStringLiteral("centerOpen")).toBool()
                && center.value(QStringLiteral("visible")).toBool()
                && center.value(QStringLiteral("active")).toBool()
                && !center.value(QStringLiteral("activeFocusItem"))
                        .toString()
                        .isEmpty()
                && (!requireInitialCloseFocus
                    || center.value(QStringLiteral("activeFocusItem"))
                           == QStringLiteral("notificationCenterCloseButton"))
                && observationCount(snapshot,
                                    QLatin1StringView("centerOpenedCount"))
                    > openedBefore;
        }, error)
        .has_value();
}

bool focusNotificationControl(QLatin1StringView objectName,
                              DevelopmentInputDriver &input,
                              NotificationLiveEvidenceClient &evidence,
                              QString *error)
{
    const auto current = evidence.snapshot(error);
    if (!current) {
        return false;
    }
    const QJsonObject center =
        windowEvidence(*current, QLatin1StringView("center"));
    const QStringList chain = names(
        center.value(QStringLiteral("forwardFocusChain")));
    qsizetype targetIndex = -1;
    for (qsizetype index = 0; index < chain.size(); ++index) {
        if (chain.at(index).startsWith(objectName)) {
            targetIndex = index;
            break;
        }
    }
    if (targetIndex < 0) {
        *error = QStringLiteral("focus chain omitted required control %1")
                     .arg(objectName);
        return false;
    }
    for (qsizetype index = 0; index < targetIndex; ++index) {
        if (!input.pressKey(QLatin1StringView("tab"), error)) {
            return false;
        }
    }
    return awaitFocus(evidence, chain.at(targetIndex), error);
}

bool exerciseCompleteNotificationFocusTraversal(
    DevelopmentInputDriver &input, NotificationLiveEvidenceClient &evidence,
    QString *error)
{
    const auto current = evidence.snapshot(error);
    if (!current) {
        return false;
    }
    const QJsonObject center =
        windowEvidence(*current, QLatin1StringView("center"));
    const QStringList forward = names(
        center.value(QStringLiteral("forwardFocusChain")));
    const QStringList reverse = names(
        center.value(QStringLiteral("reverseFocusChain")));
    const QStringList required{
        QStringLiteral("notificationCenterCloseButton"),
        QStringLiteral("notificationDoNotDisturbButton"),
        QStringLiteral("notificationClearHistoryButton"),
        QStringLiteral("notificationPrimaryAction"),
        QStringLiteral("notificationMoreActions"),
        QStringLiteral("notificationDismiss"),
        QStringLiteral("notificationSettingsRouteButton"),
    };
    if (forward.size() != reverse.size() || forward.size() < required.size()
        || forward.constFirst() != reverse.constFirst()) {
        *error = QStringLiteral("forward/reverse focus chain shapes disagree");
        return false;
    }
    for (const QString &requiredName : required) {
        if (std::none_of(forward.cbegin(), forward.cend(),
                         [&](const QString &name) {
                             return name.startsWith(requiredName);
                         })) {
            *error = QStringLiteral("focus chain omitted %1").arg(requiredName);
            return false;
        }
    }
    for (qsizetype index = 1; index < forward.size(); ++index) {
        if (reverse.at(index) != forward.at(forward.size() - index)) {
            *error = QStringLiteral(
                "reverse focus chain is not the inverse natural traversal");
            return false;
        }
    }
    return injectFocusSequence(forward, false, input, evidence, error)
        && injectFocusSequence(reverse, true, input, evidence, error);
}

} // namespace QindaQt::Test
