// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinworkspaceuiport.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class KWinWorkspaceUiPortPolicyTest final : public QObject
{
    Q_OBJECT

private slots:
    void admitsOnlyEligibleLiveWindows();
    void prefersDesktopFileNameForApplicationIdentity();
    void describesCommittedLayoutWhenPresentationFails();
};

void KWinWorkspaceUiPortPolicyTest::admitsOnlyEligibleLiveWindows()
{
    QVERIFY(workspaceWindowIsEligible({true, true, true, true}));
    QVERIFY(!workspaceWindowIsEligible({false, true, true, true}));
    QVERIFY(!workspaceWindowIsEligible({true, false, true, true}));
    QVERIFY(!workspaceWindowIsEligible({true, true, false, true}));
    QVERIFY(!workspaceWindowIsEligible({true, true, true, false}));
}

void KWinWorkspaceUiPortPolicyTest::prefersDesktopFileNameForApplicationIdentity()
{
    QCOMPARE(workspaceDesktopEntryId(QStringLiteral(" org.kde.konsole "),
                                     QStringLiteral("konsole")),
             QStringLiteral("org.kde.konsole"));
    QCOMPARE(workspaceDesktopEntryId({}, QStringLiteral(" konsole ")),
             QStringLiteral("konsole"));
    QVERIFY(workspaceDesktopEntryId({}, {}).isEmpty());
}

void KWinWorkspaceUiPortPolicyTest::describesCommittedLayoutWhenPresentationFails()
{
    const auto warning = workspacePresentationWarning(QStringLiteral("write denied"));
    QVERIFY(warning.contains(QStringLiteral("layout was restored")));
    QVERIFY(warning.contains(QStringLiteral("write denied")));
}

QTEST_APPLESS_MAIN(KWinWorkspaceUiPortPolicyTest)

#include "tst_kwinworkspaceuiportpolicy.moc"
