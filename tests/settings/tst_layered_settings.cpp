// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/settings/layered_settings.h"

#include <QtTest>

#include <optional>

using namespace QindaQt::Settings;

class LayeredSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void resolvesLayersInStablePrecedenceOrder();
    void invalidBatchIsFullyRejected();
    void staleTransactionConflicts();
    void changeSetSeparatesRawAndEffectiveChanges();
    void provenanceChangeIsObservableAndNoOpIsStable();
    void systemDefaultsAreReadOnly();

private:
    std::optional<SettingsSchema> m_schema;
};

void LayeredSettingsTests::initTestCase()
{
    QString error;
    m_schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error);
    QVERIFY2(m_schema.has_value(), qPrintable(error));
}

void LayeredSettingsTests::resolvesLayersInStablePrecedenceOrder()
{
    LayeredSettings settings(*m_schema);
    const auto key = QStringLiteral("appearance.theme");
    QCOMPARE(settings.value(key).toString(), QStringLiteral("qinda-dark"));
    QVERIFY(settings.sourceLayer(key) == std::optional(SettingLayer::SystemDefaults));

    QVERIFY(settings.replaceLayer(SettingLayer::ProfileDefaults,
                                  {{key, QStringLiteral("qinda-dusk")}})
                .ok());
    auto user = settings.beginTransaction(SettingLayer::UserOverrides);
    user.setValue(key, QStringLiteral("qinda-light"));
    QVERIFY(settings.commit(user).ok());
    auto session = settings.beginTransaction(SettingLayer::SessionOverrides);
    session.setValue(key, QStringLiteral("qinda-high-contrast"));
    QVERIFY(settings.commit(session).ok());

    QCOMPARE(settings.value(key).toString(), QStringLiteral("qinda-high-contrast"));
    QVERIFY(settings.sourceLayer(key) == std::optional(SettingLayer::SessionOverrides));
    QVERIFY(settings.clearSessionOverrides().ok());
    QCOMPARE(settings.value(key).toString(), QStringLiteral("qinda-light"));
    QVERIFY(settings.sourceLayer(key) == std::optional(SettingLayer::UserOverrides));
}

void LayeredSettingsTests::invalidBatchIsFullyRejected()
{
    LayeredSettings settings(*m_schema);
    auto transaction = settings.beginTransaction(SettingLayer::UserOverrides);
    transaction.setValue(QStringLiteral("appearance.blurEnabled"), false);
    transaction.setValue(QStringLiteral("windowManagement.snapDistance"), 999);

    const auto result = settings.commit(transaction);
    QVERIFY(result.status == CommitStatus::ValidationFailed);
    QCOMPARE(settings.revision(), quint64(0));
    QCOMPARE(settings.value(QStringLiteral("appearance.blurEnabled")).toBool(), true);
    QVERIFY(settings.layerValues(SettingLayer::UserOverrides).isEmpty());
}

void LayeredSettingsTests::staleTransactionConflicts()
{
    LayeredSettings settings(*m_schema);
    auto first = settings.beginTransaction(SettingLayer::UserOverrides);
    auto stale = settings.beginTransaction(SettingLayer::UserOverrides);
    first.setValue(QStringLiteral("appearance.blurEnabled"), false);
    stale.setValue(QStringLiteral("appearance.animationsEnabled"), false);

    QVERIFY(settings.commit(first).ok());
    const auto result = settings.commit(stale);
    QVERIFY(result.status == CommitStatus::Conflict);
    QCOMPARE(settings.value(QStringLiteral("appearance.animationsEnabled")).toBool(), true);
}

void LayeredSettingsTests::changeSetSeparatesRawAndEffectiveChanges()
{
    LayeredSettings settings(*m_schema);
    const auto key = QStringLiteral("appearance.theme");
    QVERIFY(settings.replaceLayer(SettingLayer::ProfileDefaults,
                                  {{key, QStringLiteral("qinda-dusk")}})
                .ok());
    QVERIFY(settings.replaceLayer(SettingLayer::UserOverrides,
                                  {{key, QStringLiteral("qinda-light")}})
                .ok());

    const auto hidden = settings.replaceLayer(SettingLayer::ProfileDefaults,
                                              {{key, QStringLiteral("qinda-high-contrast")}});
    QVERIFY(hidden.ok());
    QCOMPARE(hidden.changes.touchedKeys, QStringList{key});
    QVERIFY(hidden.changes.effectiveChanges.isEmpty());

    auto reveal = settings.beginTransaction(SettingLayer::UserOverrides);
    reveal.removeValue(key);
    const auto revealed = settings.commit(reveal);
    QVERIFY(revealed.ok());
    QCOMPARE(revealed.changes.effectiveChanges.size(), 1);
    QVERIFY(revealed.changes.effectiveChanges.first().currentSource
            == SettingLayer::ProfileDefaults);
    QCOMPARE(settings.value(key).toString(), QStringLiteral("qinda-high-contrast"));
}

void LayeredSettingsTests::provenanceChangeIsObservableAndNoOpIsStable()
{
    LayeredSettings settings(*m_schema);
    const auto key = QStringLiteral("appearance.blurEnabled");
    auto overrideWithSameValue = settings.beginTransaction(SettingLayer::UserOverrides);
    overrideWithSameValue.setValue(key, true);
    const auto first = settings.commit(overrideWithSameValue);
    QVERIFY(first.ok());
    QCOMPARE(first.changes.effectiveChanges.size(), 1);
    QVERIFY(first.changes.effectiveChanges.first().previousSource
            == SettingLayer::SystemDefaults);
    QVERIFY(first.changes.effectiveChanges.first().currentSource == SettingLayer::UserOverrides);
    QCOMPARE(settings.revision(), quint64(1));

    auto noOp = settings.beginTransaction(SettingLayer::UserOverrides);
    noOp.setValue(key, true);
    const auto second = settings.commit(noOp);
    QVERIFY(second.ok());
    QVERIFY(second.changes.isEmpty());
    QCOMPARE(settings.revision(), quint64(1));
}

void LayeredSettingsTests::systemDefaultsAreReadOnly()
{
    LayeredSettings settings(*m_schema);
    auto transaction = settings.beginTransaction(SettingLayer::SystemDefaults);
    transaction.setValue(QStringLiteral("appearance.theme"), QStringLiteral("replacement"));
    QVERIFY(settings.commit(transaction).status == CommitStatus::ReadOnlyLayer);
    QVERIFY(settings.replaceLayer(SettingLayer::SystemDefaults, {}).status
            == CommitStatus::ReadOnlyLayer);
    QCOMPARE(settings.revision(), quint64(0));
}

QTEST_GUILESS_MAIN(LayeredSettingsTests)
#include "tst_layered_settings.moc"
