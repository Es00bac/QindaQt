// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_identity/identity_limits.h>
#include <qindaqt/services/display_identity/identity_registry.h>

#include "support/identity_test_data.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtTest>

#include <algorithm>

using namespace QindaQt::DisplayIdentity;

namespace
{

QJsonObject legacyEntry(const QString &stableId)
{
    return {{QStringLiteral("stableId"), stableId},
            {QStringLiteral("label"), QStringLiteral("Old label")},
            {QStringLiteral("lastConnector"), QStringLiteral("DP-1")},
            {QStringLiteral("manufacturer"), QStringLiteral("QIN")},
            {QStringLiteral("model"), QStringLiteral("Panel")},
            {QStringLiteral("internal"), false},
            {QStringLiteral("seenSequence"), QStringLiteral("3")}};
}

} // namespace

class IdentityRegistryTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void migratesV1AndRoundTripsV2Canonically();
    void rejectsSchemaShapeBoundsAndAmbiguity();
    void reconcilesRenameHotplugWithoutRuntimeUuid();
    void aliasRulesAreDeterministic();
    void capacityEvictsLeastRecentlySeenDisconnectedEntry();
};

void IdentityRegistryTests::migratesV1AndRoundTripsV2Canonically()
{
    const QString stableId = QStringLiteral("edid:00112233445566778899aabbccddeeff");
    const QJsonObject legacy{{QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("outputs"),
                              QJsonArray{legacyEntry(stableId)}}};
    const RegistryResult migrated = decodeRegistry(legacy);
    QVERIFY2(migrated.succeeded(), qPrintable(migrated.reasonCode));
    QVERIFY(migrated.migrated);
    QCOMPARE(migrated.registry.schemaVersion, kRegistrySchemaVersion);
    QCOMPARE(migrated.registry.entries.first().alias, QString{});
    QCOMPARE(migrated.registry.entries.first().stableId, stableId);

    QJsonObject encoded;
    QVERIFY(encodeRegistry(migrated.registry, encoded).succeeded());
    const RegistryResult decoded = decodeRegistry(encoded);
    QVERIFY(decoded.succeeded());
    QVERIFY(!decoded.migrated);
    QCOMPARE(decoded.registry, migrated.registry);
    QJsonObject second;
    QVERIFY(encodeRegistry(decoded.registry, second).succeeded());
    QCOMPARE(QJsonDocument(second).toJson(QJsonDocument::Compact),
             QJsonDocument(encoded).toJson(QJsonDocument::Compact));
}

void IdentityRegistryTests::rejectsSchemaShapeBoundsAndAmbiguity()
{
    QJsonObject document{{QStringLiteral("schemaVersion"), 99},
                         {QStringLiteral("outputs"), QJsonArray{}}};
    QCOMPARE(decodeRegistry(document).error, RegistryError::UnsupportedSchema);
    document = {{QStringLiteral("schemaVersion"),
                 QJsonValue(static_cast<qint64>(kRegistrySchemaVersion))},
                {QStringLiteral("outputs"), QJsonObject{}}};
    QCOMPARE(decodeRegistry(document).error, RegistryError::InvalidShape);

    Registry duplicate = Test::registry();
    duplicate.entries.push_back(duplicate.entries.first());
    QCOMPARE(encodeRegistry(duplicate, document).error, RegistryError::DuplicateStableId);
    Registry ambiguous = Test::registry();
    ambiguous.entries[0].ambiguous = true;
    QCOMPARE(encodeRegistry(ambiguous, document).error, RegistryError::AmbiguousAlias);
    Registry invalid = Test::registry();
    invalid.entries[0].seenSequence = 0;
    QCOMPARE(encodeRegistry(invalid, document).error, RegistryError::InvalidEntry);
    Registry unsafe = Test::registry();
    unsafe.entries[0].label = QStringLiteral("desk\u202Ehidden");
    QCOMPARE(encodeRegistry(unsafe, document).error, RegistryError::InvalidEntry);

    Registry tooMany{.schemaVersion = kRegistrySchemaVersion, .entries = {}};
    for (qsizetype index = 0; index <= kMaxRegistryEntries; ++index) {
        RegistryEntry entry = Test::registry().entries.first();
        entry.stableId = QStringLiteral("conn:%1").arg(index);
        entry.alias.clear();
        tooMany.entries.push_back(std::move(entry));
    }
    QCOMPARE(encodeRegistry(tooMany, document).error, RegistryError::TooManyEntries);
}

void IdentityRegistryTests::reconcilesRenameHotplugWithoutRuntimeUuid()
{
    Registry original = Test::registry();
    ResolvedOutput connected{.stableId = original.entries.first().stableId,
                             .connectorName = QStringLiteral("DP-7"),
                             .source = IdentitySource::EdidIdentifier,
                             .ambiguous = false,
                             .manufacturer = QStringLiteral("QIN"),
                             .model = QStringLiteral("Panel 2"),
                             .hasSerial = true,
                             .internal = false};
    const RegistryResult reconciled = reconcileRegistry(original, {connected}, 10);
    QVERIFY(reconciled.succeeded());
    QCOMPARE(reconciled.registry.entries.size(), 1);
    QCOMPARE(reconciled.registry.entries.first().lastConnector, QStringLiteral("DP-7"));
    QCOMPARE(reconciled.registry.entries.first().seenSequence, quint64(10));
    QCOMPARE(reconciled.registry.entries.first().alias, QStringLiteral("desk"));

    connected.ambiguous = true;
    const RegistryResult ambiguity = reconcileRegistry(reconciled.registry, {connected}, 11);
    QVERIFY(ambiguity.succeeded());
    QVERIFY(ambiguity.registry.entries.first().alias.isEmpty());
    QVERIFY(ambiguity.registry.entries.first().ambiguous);

    QCOMPARE(reconcileRegistry(original, {connected, connected}, 12).error,
             RegistryError::DuplicateStableId);
}

void IdentityRegistryTests::aliasRulesAreDeterministic()
{
    Registry registry = Test::registry();
    RegistryEntry second = registry.entries.first();
    second.stableId = QStringLiteral("conn:DP-2");
    second.alias.clear();
    registry.entries.push_back(second);
    QCOMPARE(setAlias(registry, second.stableId, QStringLiteral("desk")).error,
             RegistryError::DuplicateAlias);
    QCOMPARE(setAlias(registry, second.stableId, QStringLiteral("bad alias")).error,
             RegistryError::InvalidAlias);
    QCOMPARE(setAlias(registry, QStringLiteral("unknown"), QStringLiteral("other")).error,
             RegistryError::UnknownStableId);
    second.ambiguous = true;
    registry.entries[1] = second;
    QCOMPARE(setAlias(registry, second.stableId, QStringLiteral("other")).error,
             RegistryError::AmbiguousAlias);

    registry.entries[1].ambiguous = false;
    const RegistryResult set = setAlias(registry, second.stableId, QStringLiteral("other"));
    QVERIFY(set.succeeded());
    const auto assigned = std::find_if(
        set.registry.entries.cbegin(), set.registry.entries.cend(),
        [&](const RegistryEntry &entry) { return entry.stableId == second.stableId; });
    QVERIFY(assigned != set.registry.entries.cend());
    QCOMPARE(assigned->alias, QStringLiteral("other"));
}

void IdentityRegistryTests::capacityEvictsLeastRecentlySeenDisconnectedEntry()
{
    Registry registry{.schemaVersion = kRegistrySchemaVersion, .entries = {}};
    for (qsizetype index = 0; index < kMaxRegistryEntries; ++index) {
        RegistryEntry entry = Test::registry().entries.first();
        entry.stableId = QStringLiteral("conn:old-%1").arg(index);
        entry.alias.clear();
        entry.seenSequence = static_cast<quint64>(index + 1);
        registry.entries.push_back(std::move(entry));
    }
    const ResolvedOutput newcomer{.stableId = QStringLiteral("conn:new"),
                                  .connectorName = QStringLiteral("DP-new"),
                                  .source = IdentitySource::Connector,
                                  .ambiguous = false,
                                  .manufacturer = {},
                                  .model = QStringLiteral("New"),
                                  .hasSerial = false,
                                  .internal = false};
    const RegistryResult reconciled = reconcileRegistry(registry, {newcomer}, 100);
    QVERIFY(reconciled.succeeded());
    QCOMPARE(reconciled.registry.entries.size(), kMaxRegistryEntries);
    QVERIFY(std::none_of(reconciled.registry.entries.cbegin(),
                         reconciled.registry.entries.cend(),
                         [](const RegistryEntry &entry) {
                             return entry.stableId == QStringLiteral("conn:old-0");
                         }));
    QVERIFY(std::any_of(reconciled.registry.entries.cbegin(),
                        reconciled.registry.entries.cend(),
                        [](const RegistryEntry &entry) {
                            return entry.stableId == QStringLiteral("conn:new");
                        }));
}

QTEST_GUILESS_MAIN(IdentityRegistryTests)
#include "tst_identity_registry.moc"
