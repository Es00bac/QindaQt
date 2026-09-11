// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt_platform_theme.h"
#include <QDBusConnection>
#include <QtGui/private/qgenericunixtheme_p.h>
#include <qpa/qplatformmenu.h>
#include <QtTest>

// ADR-0130 against Qt's real generic theme on a private session bus. That
// theme caches its first registrar answer for the process, so this sequence
// deliberately starts with no owner: plain delegation would pin every later
// menubar in-window, while the per-creation probe must still produce a D-Bus
// menubar once a host owns the name, and none again after it leaves.
// Registration and release are synchronous bus-daemon round trips, so no
// event-loop turn (and no Settings1 client start) happens during the test.
class PlatformMenuBarTest final : public QObject {
    Q_OBJECT
private slots:
    void menuBarsFollowTheRegistrarOwnerAtEachCreation() {
        using QindaQt::QtTheme::PlatformTheme;
        const QString registrar = QString::fromLatin1("com.canonical.AppMenu.Registrar");
        const QString hostName = QStringLiteral("qindaqt-menubar-host");
        QDBusConnection host = QDBusConnection::connectToBus(QDBusConnection::SessionBus, hostName);
        QVERIFY(host.isConnected());
        PlatformTheme theme(std::unique_ptr<QPlatformTheme>(QGenericUnixTheme::createUnixTheme(QStringLiteral("generic"))),
                            {}, &PlatformTheme::appMenuRegistrarOwned);

        QVERIFY(!PlatformTheme::appMenuRegistrarOwned());
        QCOMPARE(theme.createPlatformMenuBar(), nullptr);

        QVERIFY(host.registerService(registrar));
        QVERIFY(PlatformTheme::appMenuRegistrarOwned());
        std::unique_ptr<QPlatformMenuBar> hosted(theme.createPlatformMenuBar());
        QVERIFY(hosted != nullptr);

        QVERIFY(host.unregisterService(registrar));
        QVERIFY(!PlatformTheme::appMenuRegistrarOwned());
        QCOMPARE(theme.createPlatformMenuBar(), nullptr);

        QVERIFY(host.registerService(registrar));
        std::unique_ptr<QPlatformMenuBar> restored(theme.createPlatformMenuBar());
        QVERIFY(restored != nullptr);

        hosted.reset();
        restored.reset();
        QVERIFY(host.unregisterService(registrar));
        QDBusConnection::disconnectFromBus(hostName);
    }
};
QTEST_MAIN(PlatformMenuBarTest)
#include "tst_platform_menubar.moc"
