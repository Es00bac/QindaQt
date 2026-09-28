// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/themes/icon_theme_catalog.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::Themes;
class IconThemeCatalogTest : public QObject {
    Q_OBJECT
private slots:
    void confinedDiscoveryAndFallback() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString root = temporary.path() + "/icons";
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write(root + "/Choice/index.theme", "[Icon Theme]\nName=My icons\n"));
        QVERIFY(write(root + "/Hidden/index.theme", "[Icon Theme]\nName=Hidden\nHidden=true\n"));
        QVERIFY(write(root + "/Huge/index.theme", QByteArray(256 * 1024 + 1, 'x')));
        QVERIFY(write(temporary.path() + "/outside/index.theme", "[Icon Theme]\nName=Escape\n"));
        QVERIFY(QFile::link(temporary.path() + "/outside", root + "/Escape"));
        const auto entries = installedIconThemes({root});
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().id, "Choice");
        QCOMPARE(entries.first().name, "My icons");
        QCOMPARE(resolveIconTheme("Choice", "QindaQt", {root}), "Choice");
        QCOMPARE(resolveIconTheme("", "QindaQt", {root}), "QindaQt");
        QCOMPARE(resolveIconTheme("Missing", "QindaQt", {root}), "QindaQt");
        QCOMPARE(resolveIconTheme("../outside", "QindaQt", {root}), "QindaQt");
        QVERIFY(!safeIconThemeId("/absolute"));
        QVERIFY(!safeIconThemeId(QString(129, 'a')));
    }

    // The color theme's authored family is used only when it is installed;
    // a missing one resolves to QindaQt instead of reaching Qt as a name
    // that falls straight to hicolor. The user's installed choice still wins.
    void authoredFamilyMustBeInstalled() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString first = temporary.path() + "/first/icons";
        const QString second = temporary.path() + "/second/icons";
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write(first + "/QindaQt/index.theme", "[Icon Theme]\nName=QindaQt\n"));
        QVERIFY(write(second + "/QindaKith/index.theme", "[Icon Theme]\nName=Kith\n"));
        QVERIFY(write(first + "/Choice/index.theme", "[Icon Theme]\nName=Choice\n"));
        QVERIFY(write(first + "/Ghost/index.theme", "[Icon Theme]\nName=Ghost\nHidden=true\n"));
        QVERIFY(write(temporary.path() + "/outside/index.theme", "[Icon Theme]\nName=Out\n"));
        QVERIFY(QFile::link(temporary.path() + "/outside", first + "/Escaped"));
        const QStringList roots{first, second};
        // Installed authored family (in a later root).
        QCOMPARE(resolveIconTheme("", "QindaKith", roots), "QindaKith");
        // Missing, hidden, escaping or unsafe authored families -> QindaQt.
        QCOMPARE(resolveIconTheme("", "QindaOrbit", roots), "QindaQt");
        QCOMPARE(resolveIconTheme("", "Ghost", roots), "QindaQt");
        QCOMPARE(resolveIconTheme("", "Escaped", roots), "QindaQt");
        QCOMPARE(resolveIconTheme("", "../outside", roots), "QindaQt");
        // The user's installed choice wins over an installed authored family;
        // an uninstalled choice falls back to the authored family.
        QCOMPARE(resolveIconTheme("Choice", "QindaKith", roots), "Choice");
        QCOMPARE(resolveIconTheme("Missing", "QindaKith", roots), "QindaKith");
        QCOMPARE(resolveIconTheme("Missing", "QindaOrbit", roots), "QindaQt");
        // No roots at all: QindaQt, the one name the desktop always ships.
        QCOMPARE(resolveIconTheme("", "QindaKith", {}), "QindaQt");
    }
};
QTEST_GUILESS_MAIN(IconThemeCatalogTest)
#include "tst_icon_theme_catalog.moc"
