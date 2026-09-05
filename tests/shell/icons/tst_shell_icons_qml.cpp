// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/icons/icon_runtime.h>

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/themes/theme_loader.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtTest>

#include "shell_icons_test_fixtures.h"

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Shell::Icons;

// Offscreen compiled-module row for the QML Icon element under
// QT_FATAL_WARNINGS=1 with the host display and bus variables unset: the
// IconRuntime seam wires the provider and lookup singleton, resolved names
// render through the provider, and unresolved names render the typed
// token-colored placeholder with truthful accessible text.
class ShellIconsQmlTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void resolvedAndFallbackRows();

private:
    bool publishTokens(QQmlEngine &engine, QString *error);

    QString m_icons1;
    QString m_icons2;
};

void ShellIconsQmlTest::initTestCase()
{
    const QString base = ShellIconsTest::fixtureRoot() + QStringLiteral("/qml");
    QVERIFY2(ShellIconsTest::buildLocatorFixtures(base), "qml fixture tree");
    m_icons1 = base + QStringLiteral("/icons1");
    m_icons2 = base + QStringLiteral("/icons2");
}

// Same publication seam production composition uses (task-list precedent):
// without a published QST-1 generation the placeholder tile would read
// undefined tokens and the fatal-warnings row would abort. The facade
// singleton is per-engine, so publication targets the component's own
// engine.
bool ShellIconsQmlTest::publishTokens(QQmlEngine &engine, QString *error)
{
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    for (int spin = 0; spin < 100 && registration.status() == QQmlComponent::Loading;
         ++spin) {
        QTest::qWait(10);
    }
    if (!registration.isReady()) {
        *error = registration.errorString();
        return false;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    if (registrationObject == nullptr) {
        *error = registration.errorString();
        return false;
    }
    auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (facade == nullptr) {
        *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
        return false;
    }
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!loaded.ok) {
        *error = loaded.error;
        return false;
    }
    return facade->publish(loaded.theme, {}, error);
}

void ShellIconsQmlTest::resolvedAndFallbackRows()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_SHELL_ICONS_QML_IMPORT_PATH));
    QString error;
    QVERIFY2(publishTokens(engine, &error), qPrintable(error));
    QVERIFY(IconRuntime::install(engine, {m_icons1, m_icons2},
                                 {QStringLiteral("fixturetheme")}));

    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import QindaQt.Shell.Icons 1.0
        Item {
            property bool exactResolved: exactIcon.resolved
            property bool missingResolved: missingIcon.resolved
            property string missingAccessibleName: missingIcon.Accessible.name
            property string exactAccessibleName: exactIcon.Accessible.name
            Icon { id: exactIcon; objectName: "exactIcon"; name: "exact"; size: 16
                   fallbackText: "Foo" }
            Icon { id: missingIcon; objectName: "missingIcon"; name: "missing-icon"
                   size: 24; fallbackText: "Terminal" }
        }
    )qml",
                      QUrl(QStringLiteral("inline:icon-row.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.create());
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));

    // Resolved row: the image renders through the provider.
    QVERIFY(root->property("exactResolved").toBool());
    QObject *exactIcon = root->findChild<QObject *>(QStringLiteral("exactIcon"));
    QVERIFY(exactIcon != nullptr);
    QObject *image = exactIcon->findChild<QObject *>(QStringLiteral("iconImage"));
    QVERIFY(image != nullptr);
    QTRY_VERIFY(image->property("visible").toBool());
    QTRY_COMPARE(image->property("status").toInt(), 1); // Image.Ready
    QCOMPARE(root->property("exactAccessibleName").toString(), QStringLiteral("Foo"));

    // Unresolved row: the typed placeholder shows the fallback glyph with a
    // truthful accessible name, and no provider image is requested.
    QVERIFY(!root->property("missingResolved").toBool());
    QObject *missingIcon = root->findChild<QObject *>(QStringLiteral("missingIcon"));
    QVERIFY(missingIcon != nullptr);
    QObject *tile = missingIcon->findChild<QObject *>(QStringLiteral("placeholderTile"));
    QVERIFY(tile != nullptr);
    QVERIFY(tile->property("visible").toBool());
    QObject *glyph = missingIcon->findChild<QObject *>(QStringLiteral("placeholderGlyph"));
    QVERIFY(glyph != nullptr);
    QCOMPARE(glyph->property("text").toString(), QStringLiteral("T"));
    QCOMPARE(root->property("missingAccessibleName").toString(),
             QStringLiteral("Terminal"));
}

QTEST_MAIN(ShellIconsQmlTest)
#include "tst_shell_icons_qml.moc"
