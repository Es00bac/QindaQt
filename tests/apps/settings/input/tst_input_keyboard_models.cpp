// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_config_port.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>
#include <qindaqt/apps/settings_input/keyboard_layouts_model.h>
#include <qindaqt/apps/settings_input/keyboard_settings_model.h>

#include <QTest>

using QindaQt::Apps::SettingsInput::EvdevLayoutOption;
using QindaQt::Apps::SettingsInput::KeyboardConfig;
using QindaQt::Apps::SettingsInput::KeyboardLayoutPort;
using QindaQt::Apps::SettingsInput::KeyboardLayoutSelection;
using QindaQt::Apps::SettingsInput::KeyboardLayoutsModel;
using QindaQt::Apps::SettingsInput::KeyboardConfigPort;
using QindaQt::Apps::SettingsInput::KeyboardSettingsModel;
using QindaQt::Apps::SettingsInput::StoreResult;

namespace {

class FakeKeyboardConfigPort final : public KeyboardConfigPort
{
public:
    KeyboardConfig scripted;
    StoreResult nextResult = StoreResult::Stored;
    QString nextError;
    mutable int writes = 0;

    KeyboardConfig read(QString *error) const override
    {
        if (authorityPresent && error != nullptr) {
            error->clear();
        }
        if (!authorityPresent && error != nullptr) {
            *error = QStringLiteral("storage error");
        }
        return scripted;
    }

    StoreResult write(const KeyboardConfig &config, QString *error) const override
    {
        ++writes;
        if (nextResult == StoreResult::Failed) {
            if (error != nullptr) {
                *error = nextError.isEmpty() ? QStringLiteral("refused")
                                             : nextError;
            }
            return StoreResult::Failed;
        }
        stored = config;
        return nextResult;
    }

    bool authorityPresent = true;
    mutable KeyboardConfig stored;
};

class FakeLayoutPort final : public KeyboardLayoutPort
{
public:
    QList<KeyboardLayoutSelection> scripted;
    StoreResult nextResult = StoreResult::Stored;
    QString nextError;
    mutable int writes = 0;

    QList<KeyboardLayoutSelection>
    configuredLayouts(QString *error) const override
    {
        if (error != nullptr) {
            error->clear();
        }
        return scripted;
    }

    StoreResult
    writeConfiguredLayouts(const QList<KeyboardLayoutSelection> &layouts,
                           QString *error) const override
    {
        ++writes;
        if (nextResult == StoreResult::Failed) {
            if (error != nullptr) {
                *error = nextError.isEmpty() ? QStringLiteral("refused")
                                             : nextError;
            }
            return StoreResult::Failed;
        }
        stored = layouts;
        return nextResult;
    }

    mutable QList<KeyboardLayoutSelection> stored;
};

EvdevLayoutOption catalogOption(const QString &code,
                                const QList<QPair<QString, QString>> &variants)
{
    EvdevLayoutOption option;
    option.code = code;
    option.description = QStringLiteral("Description of %1").arg(code);
    for (const auto &variant : variants) {
        QindaQt::Apps::SettingsInput::EvdevVariantOption item;
        item.code = variant.first;
        item.description = variant.second;
        option.variants.append(item);
    }
    return option;
}

} // namespace

class KeyboardModelsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keyboardApplyMapsEveryStoreOutcome();
    void layoutsAddRemoveReorderAndApply();
    void layoutsRejectUnknownLayouts();
    void layoutsRequireCatalogBeforeAccepting();
    void layoutsLastRowCannotBeRemoved();

private:
    FakeKeyboardConfigPort m_configPort;
    FakeLayoutPort m_layoutPort;
};

void KeyboardModelsTest::keyboardApplyMapsEveryStoreOutcome()
{
    KeyboardSettingsModel model(m_configPort);
    model.setRepeatDelayMs(660);
    model.setRepeatRate(25);

    m_configPort.nextResult = StoreResult::Stored;
    model.apply();
    QCOMPARE(model.statusText(), KeyboardSettingsModel::tr("Keyboard settings applied"));
    QCOMPARE(m_configPort.stored.repeatDelayMs, 660);

    m_configPort.nextResult = StoreResult::StoredButReloadFailed;
    model.apply();
    QVERIFY(model.statusText().contains(QStringLiteral("next session")));
    QVERIFY(!model.statusText().contains(QStringLiteral("applied")));

    m_configPort.nextResult = StoreResult::Failed;
    m_configPort.nextError = QStringLiteral("disk full");
    model.apply();
    QVERIFY(model.statusText().contains(QStringLiteral("disk full")));
}

void KeyboardModelsTest::layoutsAddRemoveReorderAndApply()
{
    FakeLayoutPort port;
    KeyboardLayoutsModel model(port);
    QVERIFY(model.catalog()->load({
        catalogOption(QStringLiteral("us"), {}),
        catalogOption(QStringLiteral("de"),
                      {{QStringLiteral("nodeadkeys"),
                        QStringLiteral("German (no dead keys)")}}),
    }));
    port.scripted.append(
        {QStringLiteral("us"), QString(), QStringLiteral("English (US)"), {}});

    model.refresh();
    QCOMPARE(model.count(), 1);
    QVERIFY(model.addLayout(QStringLiteral("de"), QStringLiteral("nodeadkeys")));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.index(0, 0).data(KeyboardLayoutsModel::LayoutRole),
             QStringLiteral("us"));
    QCOMPARE(model.index(1, 0).data(KeyboardLayoutsModel::VariantRole),
             QStringLiteral("nodeadkeys"));
    QCOMPARE(model.index(1, 0).data(KeyboardLayoutsModel::TitleRole),
             QStringLiteral("Description of de"));

    model.moveUp(1);
    QCOMPARE(model.index(0, 0).data(KeyboardLayoutsModel::LayoutRole),
             QStringLiteral("de"));
    model.moveDown(0);
    QCOMPARE(model.index(0, 0).data(KeyboardLayoutsModel::LayoutRole),
             QStringLiteral("us"));

    model.removeRow(1);
    QCOMPARE(model.count(), 1);

    QVERIFY(model.addLayout(QStringLiteral("de"), QString()));
    model.apply();
    QCOMPARE(port.writes, 1);
    QCOMPARE(port.stored.size(), 2);
}

void KeyboardModelsTest::layoutsRejectUnknownLayouts()
{
    FakeLayoutPort port;
    KeyboardLayoutsModel model(port);
    QVERIFY(model.catalog()->load({catalogOption(QStringLiteral("us"), {})}));
    model.refresh();
    QVERIFY(!model.addLayout(QStringLiteral("xx"), QString()));
    QVERIFY(model.statusText().contains(QStringLiteral("Unknown layout")));
    QCOMPARE(model.count(), 0);
    // And nothing was written.
    model.apply();
    QCOMPARE(port.writes, 1);
    QVERIFY(port.stored.isEmpty());
}

void KeyboardModelsTest::layoutsRequireCatalogBeforeAccepting()
{
    FakeLayoutPort port;
    KeyboardLayoutsModel model(port);
    // No catalog loaded: nothing may pass validation.
    QVERIFY(!model.addLayout(QStringLiteral("us"), QString()));
}

void KeyboardModelsTest::layoutsLastRowCannotBeRemoved()
{
    FakeLayoutPort port;
    KeyboardLayoutsModel model(port);
    QVERIFY(model.catalog()->load({catalogOption(QStringLiteral("us"), {})}));
    port.scripted.append({QStringLiteral("us"), QString(), {}, {}});
    model.refresh();
    QCOMPARE(model.count(), 1);
    model.removeRow(0);
    QCOMPARE(model.count(), 1);
}

QTEST_MAIN(KeyboardModelsTest)
#include "tst_input_keyboard_models.moc"
