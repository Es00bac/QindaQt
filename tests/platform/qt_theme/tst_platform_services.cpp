// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt_platform_theme.h"
#include <QtTest>

class GenericServicesFixture final : public QPlatformTheme {
public:
    mutable int trays = 0;
    mutable int menuItems = 0;
    mutable int menus = 0;
    mutable int menuBars = 0;
    mutable int dialogs = 0;
    mutable DialogType dialogType = MessageDialog;
    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override { ++trays; return nullptr; }
    QPlatformMenuItem *createPlatformMenuItem() const override { ++menuItems; return nullptr; }
    QPlatformMenu *createPlatformMenu() const override { ++menus; return nullptr; }
    QPlatformMenuBar *createPlatformMenuBar() const override { ++menuBars; return nullptr; }
    bool usePlatformNativeDialog(DialogType type) const override { return type == FileDialog; }
    QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const override {
        ++dialogs; dialogType = type; return nullptr;
    }
};
class PlatformServicesTest final : public QObject {
    Q_OBJECT
private slots:
    void appearanceLeavesNativePlatformServicesWithTheirOwner() {
        auto services = std::make_unique<GenericServicesFixture>();
        auto *observed = services.get();
        QindaQt::QtTheme::PlatformTheme theme(std::move(services), {});
        QCOMPARE(theme.createPlatformSystemTrayIcon(), nullptr);
        QCOMPARE(theme.createPlatformMenuItem(), nullptr);
        QCOMPARE(theme.createPlatformMenu(), nullptr);
        QCOMPARE(theme.createPlatformMenuBar(), nullptr);
        QVERIFY(theme.usePlatformNativeDialog(QPlatformTheme::FileDialog));
        QVERIFY(!theme.usePlatformNativeDialog(QPlatformTheme::FontDialog));
        QCOMPARE(theme.createPlatformDialogHelper(QPlatformTheme::FileDialog), nullptr);
        QCOMPARE(observed->trays, 1);
        QCOMPARE(observed->menuItems, 1);
        QCOMPARE(observed->menus, 1);
        QCOMPARE(observed->menuBars, 1);
        QCOMPARE(observed->dialogs, 1);
        QCOMPARE(observed->dialogType, QPlatformTheme::FileDialog);
        // No event-loop turn is entered before destruction: the async Settings
        // observer must not outlive its theme or run on a borrowed service.
    }
};
QTEST_MAIN(PlatformServicesTest)
#include "tst_platform_services.moc"
