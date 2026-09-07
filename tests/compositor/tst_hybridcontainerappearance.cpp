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

QTEST_GUILESS_MAIN(HybridContainerAppearanceStoreTests)
#include "tst_hybridcontainerappearance.moc"
