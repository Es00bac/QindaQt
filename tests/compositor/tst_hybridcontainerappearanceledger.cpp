// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerappearance.h"
#include "hybridcontainerappearanceledger.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using QindaQt::Compositor::ContainerAppearance;
using QindaQt::Compositor::defaultContainerName;

class ContainerAppearanceLedgerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingFileLoadsEmpty();
    void roundTripsACustomNameAndLeavesTheDefaultUnset();
    void savingDropsEntriesWithNoOverride();
    void damagedFileLoadsEmptyRatherThanFailing();
    void secondSaveReplacesRatherThanMerges();

private:
    QTemporaryDir m_dir;
};

void ContainerAppearanceLedgerTests::missingFileLoadsEmpty()
{
    const ContainerAppearanceLedger ledger(
        m_dir.filePath(QStringLiteral("does-not-exist.json")));
    QVERIFY(ledger.load().isEmpty());
}

// Caveat: "a persistence round-trip that includes a custom name and the
// default." A container the user renamed comes back with that exact name;
// a container that was never renamed has nothing on disk and, once applied
// to a fresh store, still resolves to the translated default rather than an
// empty or corrupted title.
void ContainerAppearanceLedgerTests::roundTripsACustomNameAndLeavesTheDefaultUnset()
{
    const ContainerAppearanceLedger ledger(m_dir.filePath(QStringLiteral("ledger.json")));

    HybridContainerAppearanceStore live;
    QString error;
    QVERIFY(live.setName(QStringLiteral("hybrid-r1-container"),
                         QStringLiteral("Research Stack"), &error));
    QVERIFY(live.setColor(QStringLiteral("hybrid-r1-container"),
                          QStringLiteral("#30A46C"), &error));
    // hybrid-r2-container is a live container with no rename: it must not
    // appear on disk, and must not be distinguishable from "never seen".
    QVERIFY(!live.hasCustomName(QStringLiteral("hybrid-r2-container")));

    QVERIFY(ledger.save(live.snapshot()));

    const auto loaded = ledger.load();
    QCOMPARE(loaded.size(), 1);
    QVERIFY(!loaded.contains(QStringLiteral("hybrid-r2-container")));

    HybridContainerAppearanceStore restored;
    for (auto it = loaded.constBegin(); it != loaded.constEnd(); ++it) {
        if (!it.value().name.isEmpty()) {
            QVERIFY(restored.setName(it.key(), it.value().name, &error));
        }
        if (!it.value().colorHex.isEmpty()) {
            QVERIFY(restored.setColor(it.key(), it.value().colorHex, &error));
        }
    }

    QCOMPARE(restored.displayName(QStringLiteral("hybrid-r1-container")),
             QStringLiteral("Research Stack"));
    QVERIFY(restored.hasCustomName(QStringLiteral("hybrid-r1-container")));
    QCOMPARE(restored.appearance(QStringLiteral("hybrid-r1-container")).colorHex,
             QStringLiteral("#30A46C"));

    // The never-renamed container round-trips to the same default text as a
    // container the ledger has never heard of, never to an empty title.
    QCOMPARE(restored.displayName(QStringLiteral("hybrid-r2-container")),
             defaultContainerName());
    QVERIFY(!restored.hasCustomName(QStringLiteral("hybrid-r2-container")));
}

void ContainerAppearanceLedgerTests::savingDropsEntriesWithNoOverride()
{
    const ContainerAppearanceLedger ledger(m_dir.filePath(QStringLiteral("sparse.json")));
    QHash<QString, ContainerAppearance> entries;
    entries.insert(QStringLiteral("blank"), ContainerAppearance{});
    QVERIFY(ledger.save(entries));
    QVERIFY(ledger.load().isEmpty());
}

void ContainerAppearanceLedgerTests::damagedFileLoadsEmptyRatherThanFailing()
{
    const QString path = m_dir.filePath(QStringLiteral("damaged.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ not json");
    file.close();

    const ContainerAppearanceLedger ledger(path);
    QVERIFY(ledger.load().isEmpty());
}

void ContainerAppearanceLedgerTests::secondSaveReplacesRatherThanMerges()
{
    const ContainerAppearanceLedger ledger(m_dir.filePath(QStringLiteral("replace.json")));
    QHash<QString, ContainerAppearance> first;
    first.insert(QStringLiteral("alpha"), ContainerAppearance{QStringLiteral("Alpha"), {}});
    QVERIFY(ledger.save(first));

    QHash<QString, ContainerAppearance> second;
    second.insert(QStringLiteral("beta"), ContainerAppearance{QStringLiteral("Beta"), {}});
    QVERIFY(ledger.save(second));

    const auto loaded = ledger.load();
    QVERIFY(!loaded.contains(QStringLiteral("alpha")));
    QCOMPARE(loaded.value(QStringLiteral("beta")).name, QStringLiteral("Beta"));
}

QTEST_GUILESS_MAIN(ContainerAppearanceLedgerTests)
#include "tst_hybridcontainerappearanceledger.moc"
