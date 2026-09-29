// SPDX-License-Identifier: GPL-3.0-or-later
#include "agent_identity.h"

#include <QtTest>

using namespace QindaQt::Apps::PolkitAgent;

class AgentIdentityTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rootIsAlwaysAdministrator();
    void fullNameAndLoginCombine();
    void loginAloneWhenNameIsUnknown();
    void bothUnknownFallsBackHonestly();
    void preferredIndexPicksTheCurrentUser();
    void preferredIndexFallsBackToFirstIdentity();
    void preferredIndexIsMinusOneWhenPolkitOffersNoIdentity();
};

void AgentIdentityTest::rootIsAlwaysAdministrator()
{
    QCOMPARE(formatUserIdentityLabel(0, QStringLiteral("root"), QStringLiteral("root")),
             QStringLiteral("Administrator (root)"));
    // Even a misleading gecos field never overrides uid 0's fixed label.
    QCOMPARE(formatUserIdentityLabel(0, QStringLiteral("toor"), QStringLiteral("Not Root At All")),
             QStringLiteral("Administrator (root)"));
}

void AgentIdentityTest::fullNameAndLoginCombine()
{
    QCOMPARE(formatUserIdentityLabel(1000, QStringLiteral("jarrod"), QStringLiteral("Jarrod C")),
             QStringLiteral("Jarrod C (jarrod)"));
}

void AgentIdentityTest::loginAloneWhenNameIsUnknown()
{
    QCOMPARE(formatUserIdentityLabel(1000, QStringLiteral("jarrod"), QString()),
             QStringLiteral("jarrod"));
}

void AgentIdentityTest::bothUnknownFallsBackHonestly()
{
    QCOMPARE(formatUserIdentityLabel(4242, QString(), QString()), QStringLiteral("User 4242"));
}

void AgentIdentityTest::preferredIndexPicksTheCurrentUser()
{
    const QList<AgentIdentity> identities{
        {QStringLiteral("Administrator (root)"), QStringLiteral("unix-user:0"), false},
        {QStringLiteral("Jarrod C (jarrod)"), QStringLiteral("unix-user:1000"), true},
    };
    QCOMPARE(selectPreferredIdentityIndex(identities), 1);
}

void AgentIdentityTest::preferredIndexFallsBackToFirstIdentity()
{
    const QList<AgentIdentity> identities{
        {QStringLiteral("Administrator (root)"), QStringLiteral("unix-user:0"), false},
        {QStringLiteral("Guest (guest)"), QStringLiteral("unix-user:1001"), false},
    };
    QCOMPARE(selectPreferredIdentityIndex(identities), 0);
}

void AgentIdentityTest::preferredIndexIsMinusOneWhenPolkitOffersNoIdentity()
{
    QCOMPARE(selectPreferredIdentityIndex({}), -1);
}

QTEST_MAIN(AgentIdentityTest)
#include "tst_agent_identity.moc"
