// SPDX-License-Identifier: GPL-3.0-or-later
#include "state_card_accessibility_test.h"

#include "control_test_support.h"

#include <QAccessible>
#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QVariant>

#include <functional>

namespace QindaQt::Controls::TestSupport {

void verifyStateCardAnnouncements(QObject *stateCard)
{
    QSignalSpy announcements(
        stateCard, SIGNAL(accessibilityAnnouncementRequested(QString,int,int)));
    QVERIFY(stateCard->property("politeAnnouncement").toInt()
            != stateCard->property("assertiveAnnouncement").toInt());
    QVERIFY(!stateCard->property("accessibilityReady").isValid());
    QVERIFY(!stateCard->property("accessibilityRevision").isValid());

    const auto requireLatest = [&](const std::function<void()> &mutate,
                                   int status,
                                   QAccessible::Role role,
                                   const QString &expected,
                                   bool assertive) {
        announcements.clear();
        mutate();
        QCOMPARE(announcements.size(), 0);
        QTRY_COMPARE(announcements.size(), 1);
        // AGENT-GUARD: One extra event drain rejects duplicate timers after the
        // first observed announcement; QTRY_COMPARE alone could return early.
        QCoreApplication::processEvents();
        QCOMPARE(announcements.size(), 1);
        QCOMPARE(accessible(stateCard)->role(), role);
        const QList<QVariant> tuple = announcements.constLast();
        QCOMPARE(tuple.at(0).toString(), expected);
        QCOMPARE(tuple.at(1).toInt(), status);
        const char *mapping = assertive ? "assertiveAnnouncement" : "politeAnnouncement";
        QCOMPARE(tuple.at(2).toInt(), stateCard->property(mapping).toInt());
    };

    const QString title = stateCard->property("title").toString();
    const auto transition = [&](int status,
                                QAccessible::Role role,
                                const QString &statusName,
                                bool assertive) {
        const QString message = stateCard->property("message").toString();
        requireLatest(
            [=]() { stateCard->setProperty("status", status); },
            status,
            role,
            QStringLiteral("%1: %2 — %3").arg(statusName, title, message),
            assertive);
    };

    transition(0, QAccessible::StaticText, QStringLiteral("Information"), false);
    transition(1, QAccessible::StaticText, QStringLiteral("Success"), false);
    transition(4, QAccessible::StaticText, QStringLiteral("Busy"), false);
    transition(2, QAccessible::AlertMessage, QStringLiteral("Warning"), true);
    transition(3, QAccessible::AlertMessage, QStringLiteral("Error"), true);
    transition(0, QAccessible::StaticText, QStringLiteral("Information"), false);

    transition(2, QAccessible::AlertMessage, QStringLiteral("Warning"), true);
    const QString warningDetail = QStringLiteral("A newer warning detail.");
    requireLatest(
        [=]() { stateCard->setProperty("message", warningDetail); },
        2,
        QAccessible::AlertMessage,
        QStringLiteral("Warning: %1 — %2").arg(title, warningDetail),
        true);

    transition(3, QAccessible::AlertMessage, QStringLiteral("Error"), true);
    const QString errorDetail = QStringLiteral("A newer error detail.");
    requireLatest(
        [=]() { stateCard->setProperty("message", errorDetail); },
        3,
        QAccessible::AlertMessage,
        QStringLiteral("Error: %1 — %2").arg(title, errorDetail),
        true);

    const QString latestTitle = QStringLiteral("Latest warning title");
    const QString latestMessage = QStringLiteral("Latest warning explanation.");
    requireLatest(
        [=]() {
            stateCard->setProperty("status", 2); // StateCard.Warning
            stateCard->setProperty("title", latestTitle);
            stateCard->setProperty("message", latestMessage);
        },
        2,
        QAccessible::AlertMessage,
        QStringLiteral("Warning: %1 — %2").arg(latestTitle, latestMessage),
        true);
}

} // namespace QindaQt::Controls::TestSupport
