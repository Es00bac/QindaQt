// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerappearance.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using QindaQt::Compositor::ContainerAppearance;

class HybridContainerAppearanceStoreTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unknownContainerHasNoOverride();
    void setNameAppliesNormalizationAndClearsOnBlank();
    void setNameRejectsInvalidTextAndLeavesPriorValue();
    void setColorAppliesNormalizationAndClearsOnBlank();
    void setColorRejectsInvalidHexAndLeavesPriorValue();
    void forgetContainerRemovesBothFields();
    void containersAreIndependent();
    void displayNameGeneratesStableNamesAndHonorsOverrides();
};

void HybridContainerAppearanceStoreTests::unknownContainerHasNoOverride()
{
    HybridContainerAppearanceStore store;
    QCOMPARE(store.appearance(QStringLiteral("missing")), ContainerAppearance{});
}

void HybridContainerAppearanceStoreTests::setNameAppliesNormalizationAndClearsOnBlank()
{
    HybridContainerAppearanceStore store;
    QString error;
    QVERIFY(store.setName(QStringLiteral("group"), QStringLiteral("  Research Stack  "),
                          &error));
    QCOMPARE(store.appearance(QStringLiteral("group")).name,
             QStringLiteral("Research Stack"));

    QVERIFY(store.setName(QStringLiteral("group"), QStringLiteral("   "), &error));
    QVERIFY(store.appearance(QStringLiteral("group")).name.isEmpty());
}

void HybridContainerAppearanceStoreTests::setNameRejectsInvalidTextAndLeavesPriorValue()
{
    HybridContainerAppearanceStore store;
    QString error;
    QVERIFY(store.setName(QStringLiteral("group"), QStringLiteral("Research Stack"), &error));
    QVERIFY(!store.setName(QStringLiteral("group"), QStringLiteral("bad\tname"), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.appearance(QStringLiteral("group")).name,
             QStringLiteral("Research Stack"));
}

void HybridContainerAppearanceStoreTests::setColorAppliesNormalizationAndClearsOnBlank()
{
    HybridContainerAppearanceStore store;
    QString error;
    QVERIFY(store.setColor(QStringLiteral("group"), QStringLiteral(" #0091ff "), &error));
    QCOMPARE(store.appearance(QStringLiteral("group")).colorHex,
             QStringLiteral("#0091FF"));

    QVERIFY(store.setColor(QStringLiteral("group"), QString{}, &error));
    QVERIFY(store.appearance(QStringLiteral("group")).colorHex.isEmpty());
}

void HybridContainerAppearanceStoreTests::setColorRejectsInvalidHexAndLeavesPriorValue()
{
    HybridContainerAppearanceStore store;
    QString error;
    QVERIFY(store.setColor(QStringLiteral("group"), QStringLiteral("#0091FF"), &error));
    QVERIFY(!store.setColor(QStringLiteral("group"), QStringLiteral("not-a-color"), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.appearance(QStringLiteral("group")).colorHex,
             QStringLiteral("#0091FF"));
}

void HybridContainerAppearanceStoreTests::forgetContainerRemovesBothFields()
{
    HybridContainerAppearanceStore store;
    QString error;
    QVERIFY(store.setName(QStringLiteral("group"), QStringLiteral("Research Stack"), &error));
    QVERIFY(store.setColor(QStringLiteral("group"), QStringLiteral("#0091FF"), &error));
    store.forgetContainer(QStringLiteral("group"));
    QCOMPARE(store.appearance(QStringLiteral("group")), ContainerAppearance{});
}

void HybridContainerAppearanceStoreTests::containersAreIndependent()
{
    HybridContainerAppearanceStore store;
    QString error;
    QVERIFY(store.setName(QStringLiteral("alpha"), QStringLiteral("Alpha"), &error));
    QVERIFY(store.setColor(QStringLiteral("beta"), QStringLiteral("#30A46C"), &error));
    QCOMPARE(store.appearance(QStringLiteral("alpha")).name, QStringLiteral("Alpha"));
    QVERIFY(store.appearance(QStringLiteral("alpha")).colorHex.isEmpty());
    QVERIFY(store.appearance(QStringLiteral("beta")).name.isEmpty());
    QCOMPARE(store.appearance(QStringLiteral("beta")).colorHex, QStringLiteral("#30A46C"));

    store.clear();
    QCOMPARE(store.appearance(QStringLiteral("alpha")), ContainerAppearance{});
    QCOMPARE(store.appearance(QStringLiteral("beta")), ContainerAppearance{});
}

void HybridContainerAppearanceStoreTests::displayNameGeneratesStableNamesAndHonorsOverrides()
{
    HybridContainerAppearanceStore store;
    QString error;

    // First observation assigns sequential numbers; repeat queries stay
    // stable for the container's lifetime (ADR-0163).
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Container 1"));
    QCOMPARE(store.displayName(QStringLiteral("beta")), QStringLiteral("Container 2"));
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Container 1"));

    // The rename override wins, and clearing it falls back to the memoized
    // generated name instead of minting a new number.
    QVERIFY(store.setName(QStringLiteral("alpha"), QStringLiteral("Games"), &error));
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Games"));
    QVERIFY(store.setName(QStringLiteral("alpha"), QStringLiteral("   "), &error));
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Container 1"));

    // Forgetting a container retires its memoized name; the counter never
    // reuses a number, so two live containers can never share a name.
    store.forgetContainer(QStringLiteral("alpha"));
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Container 3"));
    QCOMPARE(store.displayName(QStringLiteral("beta")), QStringLiteral("Container 2"));

    // clear() resets the whole generated map; the counter keeps advancing.
    store.clear();
    QCOMPARE(store.displayName(QStringLiteral("gamma")), QStringLiteral("Container 4"));
}

QTEST_GUILESS_MAIN(HybridContainerAppearanceStoreTests)
#include "tst_hybridcontainerappearance.moc"
