// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/controls/application_icon.h>
#include <QIcon>
#include <QImage>
#include <QtTest>

class ApplicationIconsTest final : public QObject {
    Q_OBJECT
private slots:
    void embeddedCatalogWorksWithoutHostTheme() {
        QIcon::setThemeName(QStringLiteral("qindaqt-test-missing-theme"));
        QIcon::setThemeSearchPaths({});
        for (const auto *name : {"go-previous-symbolic", "folder", "text-x-generic",
                "image-x-generic", "application-menu-symbolic", "emblem-symbolic-link",
                "folder-new-symbolic", "application-x-archive"}) {
            const auto icon = QindaQt::Controls::applicationIcon(QLatin1String(name));
            QVERIFY2(!icon.isNull(), name);
            const QImage pixels = icon.pixmap(64, 64).toImage();
            QVERIFY2(!pixels.isNull(), name);
            int visible = 0;
            int transparent = 0;
            for (int y=0; y<pixels.height(); ++y)
                for (int x=0; x<pixels.width(); ++x) {
                    if (pixels.pixelColor(x,y).alpha()) ++visible;
                    else ++transparent;
                }
            QVERIFY2(visible > 20, name);
            QVERIFY2(transparent > 20, name);
        }
    }
    void namesAreNotFilesystemPaths() {
        for (const QString &name : {QString(), QStringLiteral("/etc/passwd"),
             QStringLiteral("../folder"), QStringLiteral("file:folder"),
             QString(129, QLatin1Char('a')), QStringLiteral("qindaqt-does-not-exist")})
            QVERIFY(QindaQt::Controls::applicationIcon(name).isNull());
    }
};
QTEST_MAIN(ApplicationIconsTest)
#include "tst_application_icons.moc"
