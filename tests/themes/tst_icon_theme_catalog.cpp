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
};
QTEST_GUILESS_MAIN(IconThemeCatalogTest)
#include "tst_icon_theme_catalog.moc"
