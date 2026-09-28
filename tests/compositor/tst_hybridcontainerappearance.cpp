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
    void displayNameIsTheRenameOrTheDefault();
    void displayNameIsAPureRead();
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

// ADR-0281: a container is titled by its own name: the user's rename, else
// the default "Container". Never a number, never a page title.
void HybridContainerAppearanceStoreTests::displayNameIsTheRenameOrTheDefault()
{
    HybridContainerAppearanceStore store;
    QString error;
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Container"));
    QCOMPARE(store.displayName(QStringLiteral("beta")), QStringLiteral("Container"));
    QVERIFY(!store.hasCustomName(QStringLiteral("alpha")));

    QVERIFY(store.setName(QStringLiteral("alpha"), QStringLiteral("  Games "), &error));
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Games"));
    QVERIFY(store.hasCustomName(QStringLiteral("alpha")));
    QCOMPARE(store.displayName(QStringLiteral("beta")), QStringLiteral("Container"));

    // Clearing the rename returns to the default; so does forgetting.
    QVERIFY(store.setName(QStringLiteral("alpha"), QStringLiteral("   "), &error));
    QCOMPARE(store.displayName(QStringLiteral("alpha")), QStringLiteral("Container"));
    QVERIFY(!store.hasCustomName(QStringLiteral("alpha")));
    QVERIFY(store.setName(QStringLiteral("beta"), QStringLiteral("Work"), &error));
    store.forgetContainer(QStringLiteral("beta"));
    QCOMPARE(store.displayName(QStringLiteral("beta")), QStringLiteral("Container"));
}

// Reading a name never changes it: a const read, identical however often and
// in whatever order containers are inspected.
void HybridContainerAppearanceStoreTests::displayNameIsAPureRead()
{
    const HybridContainerAppearanceStore store;
    for (int pass = 0; pass < 3; ++pass) {
        QCOMPARE(store.displayName(QStringLiteral("c%1").arg(pass)), QStringLiteral("Container"));
    }
    QCOMPARE(QindaQt::Compositor::defaultContainerName(), QStringLiteral("Container"));
}

QTEST_GUILESS_MAIN(HybridContainerAppearanceStoreTests)
#include "tst_hybridcontainerappearance.moc"
