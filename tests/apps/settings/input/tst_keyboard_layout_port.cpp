// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/evdev_layout_catalog.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>

#include <QtDBus/QDBusConnection>
#include <QTemporaryDir>
#include <QTest>

using QindaQt::Apps::SettingsInput::EvdevLayoutOption;
using QindaQt::Apps::SettingsInput::KeyboardLayoutSelection;
using QindaQt::Apps::SettingsInput::QtKeyboardLayoutPort;
using QindaQt::Apps::SettingsInput::StoreResult;

class KeyboardLayoutPortTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingFileIsEmptyList();
    void layoutListRoundTrip();
    void malformedRowsAreDropped();
    void refusesEmptyDuplicateAndInvalidLists();
    void reloadFailureIsReportedSeparately();
    void catalogParsesLayoutsAndVariants();
    void catalogRejectsHostileInput();

private:
    QString path(const char *name) const
    {
        return m_dir.filePath(QLatin1String(name));
    }
    bool writeFile(const QString &path, const QByteArray &contents) const
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write(contents) >= 0;
    }
    QTemporaryDir m_dir;
};

void KeyboardLayoutPortTest::missingFileIsEmptyList()
{
    QtKeyboardLayoutPort port(path("absent-kxkbrc"),
                              QDBusConnection::sessionBus());
    QString error;
    QVERIFY(port.configuredLayouts(&error).isEmpty());
    QVERIFY(error.isEmpty());
}

void KeyboardLayoutPortTest::layoutListRoundTrip()
{
    const QString kxkbrc = path("roundtrip-kxkbrc");
    QtKeyboardLayoutPort port(kxkbrc, QDBusConnection::sessionBus());
    const QList<KeyboardLayoutSelection> layouts = {
        {QStringLiteral("us"), QString(), {}, {}},
        {QStringLiteral("de"), QStringLiteral("nodeadkeys"), {}, {}},
    };
    QString error;
    QCOMPARE(port.writeConfiguredLayouts(layouts, &error),
             StoreResult::StoredButReloadFailed);
    QCOMPARE(error, QString());

    QtKeyboardLayoutPort reader(kxkbrc, QDBusConnection::sessionBus());
    const QList<KeyboardLayoutSelection> readBack =
        reader.configuredLayouts(&error);
    QCOMPARE(error, QString());
    QCOMPARE(readBack.size(), 2);
    QCOMPARE(readBack.at(0).layout, QStringLiteral("us"));
    QCOMPARE(readBack.at(0).variant, QString());
    QCOMPARE(readBack.at(1).layout, QStringLiteral("de"));
    QCOMPARE(readBack.at(1).variant, QStringLiteral("nodeadkeys"));
}

void KeyboardLayoutPortTest::malformedRowsAreDropped()
{
    const QString kxkbrc = path("malformed-kxkbrc");
    QVERIFY(writeFile(kxkbrc,
                      "[Layout]\nLayoutList=us,bad/row,de\n"
                      "VariantList=,x,deadkeys\n"));
    QtKeyboardLayoutPort port(kxkbrc, QDBusConnection::sessionBus());
    QString error;
    const QList<KeyboardLayoutSelection> layouts =
        port.configuredLayouts(&error);
    QCOMPARE(layouts.size(), 2);
    QCOMPARE(layouts.at(0).layout, QStringLiteral("us"));
    QCOMPARE(layouts.at(1).layout, QStringLiteral("de"));
    // The malformed row dropped its variant too; the survivor keeps its own.
    QCOMPARE(layouts.at(1).variant, QStringLiteral("deadkeys"));
}

void KeyboardLayoutPortTest::refusesEmptyDuplicateAndInvalidLists()
{
    const QString kxkbrc = path("refusal-kxkbrc");
    QtKeyboardLayoutPort port(kxkbrc, QDBusConnection::sessionBus());
    QString error;
    QCOMPARE(port.writeConfiguredLayouts({}, &error), StoreResult::Failed);
    QVERIFY(error.contains(QStringLiteral("between 1 and")));

    const QList<KeyboardLayoutSelection> duplicated = {
        {QStringLiteral("us"), QString(), {}, {}},
        {QStringLiteral("us"), QStringLiteral("nodeadkeys"), {}, {}},
    };
    QCOMPARE(port.writeConfiguredLayouts(duplicated, &error),
             StoreResult::Failed);
    QVERIFY(error.contains(QStringLiteral("duplicate")));

    const QList<KeyboardLayoutSelection> hostile = {
        {QStringLiteral("../etc"), QString(), {}, {}},
    };
    QCOMPARE(port.writeConfiguredLayouts(hostile, &error),
             StoreResult::Failed);
    QVERIFY(!QFile::exists(kxkbrc));
}

void KeyboardLayoutPortTest::reloadFailureIsReportedSeparately()
{
    QtKeyboardLayoutPort port(path("no-reload-kxkbrc"),
                              QDBusConnection(QStringLiteral("none")));
    const QList<KeyboardLayoutSelection> layouts = {
        {QStringLiteral("us"), QString(), {}, {}},
    };
    QString error;
    QCOMPARE(port.writeConfiguredLayouts(layouts, &error),
             StoreResult::StoredButReloadFailed);
    QCOMPARE(error, QString());
}

void KeyboardLayoutPortTest::catalogParsesLayoutsAndVariants()
{
    const QString catalog = path("evdev.xml");
    QVERIFY(writeFile(catalog,
                      "<?xml version=\"1.0\"?>\n"
                      "<xkbConfigRegistry version=\"1.1\">\n"
                      "  <layoutList>\n"
                      "    <layout>\n"
                      "      <configItem>\n"
                      "        <name>us</name>\n"
                      "        <description>English (US)</description>\n"
                      "      </configItem>\n"
                      "      <variantList>\n"
                      "        <variant>\n"
                      "          <configItem>\n"
                      "            <name>intl</name>\n"
                      "            <description>English (US, intl.)</description>\n"
                      "          </configItem>\n"
                      "        </variant>\n"
                      "      </variantList>\n"
                      "    </layout>\n"
                      "    <layout>\n"
                      "      <configItem>\n"
                      "        <name>de</name>\n"
                      "        <description>German</description>\n"
                      "      </configItem>\n"
                      "      <variantList/>\n"
                      "    </layout>\n"
                      "  </layoutList>\n"
                      "</xkbConfigRegistry>\n"));
    QString error;
    const QList<EvdevLayoutOption> layouts =
        QindaQt::Apps::SettingsInput::EvdevLayoutCatalog::parse(catalog,
                                                                &error);
    QCOMPARE(error, QString());
    QCOMPARE(layouts.size(), 2);
    QCOMPARE(layouts.at(0).code, QStringLiteral("us"));
    QCOMPARE(layouts.at(0).description, QStringLiteral("English (US)"));
    QCOMPARE(layouts.at(0).variants.size(), 1);
    QCOMPARE(layouts.at(0).variants.first().code, QStringLiteral("intl"));
    QCOMPARE(layouts.at(1).code, QStringLiteral("de"));
    QVERIFY(layouts.at(1).variants.isEmpty());
}

void KeyboardLayoutPortTest::catalogRejectsHostileInput()
{
    QString error;
    // Absent file.
    QVERIFY(QindaQt::Apps::SettingsInput::EvdevLayoutCatalog::parse(
                path("absent.xml"), &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    // Not XML at all.
    QVERIFY(writeFile(path("binary.xml"), QByteArray("\x01\x02not-xml")));
    QVERIFY(QindaQt::Apps::SettingsInput::EvdevLayoutCatalog::parse(
                path("binary.xml"), &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    // DOCTYPE with an expansion bomb: rejected up front, never parsed.
    QVERIFY(writeFile(
        path("bomb.xml"),
        "<?xml version=\"1.0\"?>\n<!DOCTYPE lolz [\n"
        "  <!ENTITY lol \"lol\">\n  <!ENTITY lol2 \"&lol;&lol;&lol;&lol;\">\n"
        "]>\n<xkbConfigRegistry><layoutList>&lol2;</layoutList>"
        "</xkbConfigRegistry>\n"));
    QVERIFY(QindaQt::Apps::SettingsInput::EvdevLayoutCatalog::parse(
                path("bomb.xml"), &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    // Identifier far beyond the bounded length.
    const QString longName(200, 'x');
    QVERIFY(writeFile(
        path("long.xml"),
        QString(QStringLiteral(
                    "<xkbConfigRegistry><layoutList><layout>"
                    "<configItem><name>%1</name></configItem>"
                    "</layout></layoutList></xkbConfigRegistry>"))
            .arg(longName)
            .toUtf8()));
    QVERIFY(QindaQt::Apps::SettingsInput::EvdevLayoutCatalog::parse(
                path("long.xml"), &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    // Endless nesting inside one layout entry: bounded element churn.
    QByteArray endless;
    endless += "<xkbConfigRegistry><layoutList><layout>";
    for (int i = 0; i < 5000; ++i) {
        endless += "<deep>";
    }
    endless += "</layout></layoutList></xkbConfigRegistry>";
    QVERIFY(writeFile(path("deep.xml"), endless));
    QVERIFY(QindaQt::Apps::SettingsInput::EvdevLayoutCatalog::parse(
                path("deep.xml"), &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(KeyboardLayoutPortTest)
#include "tst_keyboard_layout_port.moc"
