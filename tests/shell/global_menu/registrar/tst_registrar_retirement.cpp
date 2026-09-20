// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu::Registrar;

class RegistrarRetirementTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ownerLossPreservesRemovedValues_data();
    void ownerLossPreservesRemovedValues();
};

void RegistrarRetirementTest::ownerLossPreservesRemovedValues_data()
{
    QTest::addColumn<quint32>("windowsPerOwner");
    QTest::newRow("last-window") << quint32{1};
    QTest::newRow("several-windows") << quint32{16};
    QTest::newRow("full-registry") << quint32{256};
}

void RegistrarRetirementTest::ownerLossPreservesRemovedValues()
{
    QFETCH(quint32, windowsPerOwner);
    RegistrarRegistry registry;
    QSignalSpy removed(&registry, &RegistrarRegistry::windowUnregistered);
    QSignalSpy absent(&registry, &RegistrarRegistry::ownerBecameAbsent);
    constexpr quint32 ownerCount = 4;

    for (quint32 owner = 1; owner <= ownerCount; ++owner) {
        for (quint32 window = 1; window <= windowsPerOwner; ++window) {
            // AGENT-GUARD: keep no returned registration or implicitly shared
            // owner string alive. Owner-loss cleanup must own its signal data
            // after removing the hash entry, without help from this fixture.
            QCOMPARE(registry.registerWindow(
                         (owner - 1) * windowsPerOwner + window,
                         QStringLiteral(":1.%1").arg(owner),
                         QDBusObjectPath(QStringLiteral("/Menu%1").arg(window))).outcome,
                     RegistrationOutcome::Registered);
        }
    }

    for (quint32 owner = 1; owner <= ownerCount; ++owner) {
        const QString expectedOwner = QStringLiteral(":1.%1").arg(owner);
        const auto generation = registry.ownerGeneration(expectedOwner);
        QVERIFY(generation.has_value());
        QCOMPARE(registry.retireOwner(expectedOwner, *generation), RemovalOutcome::Removed);
        QCOMPARE(removed.size(), static_cast<qsizetype>(windowsPerOwner));
        QCOMPARE(absent.size(), 1);
        QCOMPARE(absent.first().first().toString(), expectedOwner);
        QVERIFY(!registry.ownerGeneration(expectedOwner).has_value());
        QCOMPARE(registry.size(), static_cast<qsizetype>((ownerCount - owner) * windowsPerOwner));
        for (quint32 window = 1; window <= windowsPerOwner; ++window) {
            const quint32 windowId = (owner - 1) * windowsPerOwner + window;
            QCOMPARE(removed.at(window - 1).at(0).toUInt(), windowId);
            QCOMPARE(removed.at(window - 1).at(1).toString(), expectedOwner);
            QVERIFY(!registry.registrationFor(windowId).has_value());
        }
        for (const auto &menu : registry.menus()) {
            const quint32 remainingOwner = (menu.windowId - 1) / windowsPerOwner + 1;
            QVERIFY(remainingOwner > owner);
            QCOMPARE(menu.service, QStringLiteral(":1.%1").arg(remainingOwner));
        }
        removed.clear();
        absent.clear();
    }
}

QTEST_GUILESS_MAIN(RegistrarRetirementTest)

#include "tst_registrar_retirement.moc"
