// SPDX-License-Identifier: GPL-3.0-or-later
#include "containercloseprompt.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

QAbstractButton *buttonWithText(QMessageBox &prompt, const QString &text)
{
    for (auto *button : prompt.buttons()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

} // namespace

class ContainerClosePromptTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsInvalidAndDeduplicatesRequests();
    void returnsEveryExplicitDecision();
    void cancellationDuringShutdownIsSilent();
};

void ContainerClosePromptTest::rejectsInvalidAndDeduplicatesRequests()
{
    ContainerClosePrompt prompt({});
    QVERIFY(!prompt.request({}, 2));
    QVERIFY(!prompt.request(QStringLiteral("group"), 1));
    QVERIFY(prompt.request(QStringLiteral("group"), 3));
    auto *const first = prompt.promptWindow(QStringLiteral("group"));
    QVERIFY(first);
    QVERIFY(prompt.request(QStringLiteral("group"), 3));
    QCOMPARE(prompt.promptWindow(QStringLiteral("group")), first);
    QCOMPARE(prompt.activePromptCount(), 1);
}

void ContainerClosePromptTest::returnsEveryExplicitDecision()
{
    for (const auto &row : {
             std::pair{QStringLiteral("Close All"), ContainerCloseDecision::CloseAll},
             std::pair{QStringLiteral("Ungroup"), ContainerCloseDecision::Ungroup},
             std::pair{QStringLiteral("Cancel"), ContainerCloseDecision::Cancel},
         }) {
        QVector<std::pair<QString, ContainerCloseDecision>> decisions;
        ContainerClosePrompt owner(
            [&decisions](const QString &id, ContainerCloseDecision decision) {
                decisions.append({id, decision});
            });
        QVERIFY(owner.request(QStringLiteral("group"), 2));
        auto *const dialog = qobject_cast<QMessageBox *>(
            owner.promptWindow(QStringLiteral("group")));
        QVERIFY(dialog);
        auto *const button = buttonWithText(*dialog, row.first);
        QVERIFY2(button, qPrintable(row.first));
        button->click();
        QTRY_COMPARE(decisions.size(), 1);
        QCOMPARE(decisions.constFirst().first, QStringLiteral("group"));
        QCOMPARE(decisions.constFirst().second, row.second);
        QCOMPARE(owner.activePromptCount(), 0);
    }
}

void ContainerClosePromptTest::cancellationDuringShutdownIsSilent()
{
    int decisions = 0;
    ContainerClosePrompt prompt(
        [&decisions](const QString &, ContainerCloseDecision) { ++decisions; });
    QVERIFY(prompt.request(QStringLiteral("group-a"), 2));
    QVERIFY(prompt.request(QStringLiteral("group-b"), 4));
    prompt.cancelAll();
    QCoreApplication::processEvents();
    QCOMPARE(prompt.activePromptCount(), 0);
    QCOMPARE(decisions, 0);
}

QTEST_MAIN(ContainerClosePromptTest)
#include "tst_containercloseprompt.moc"
