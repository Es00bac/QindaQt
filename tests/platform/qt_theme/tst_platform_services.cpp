// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt_platform_theme.h"
#include <qpa/qplatformmenu.h>
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
class FakeMenuBar final : public QPlatformMenuBar {
public:
    void insertMenu(QPlatformMenu *, QPlatformMenu *) override {}
    void removeMenu(QPlatformMenu *) override {}
    void syncMenu(QPlatformMenu *) override {}
    void handleReparent(QWindow *) override {}
    QPlatformMenu *menuForTag(quintptr) const override { return nullptr; }
};
class MenuBarServicesFixture final : public QPlatformTheme {
public:
    mutable int menuBars = 0;
    QPlatformMenuBar *createPlatformMenuBar() const override { ++menuBars; return new FakeMenuBar; }
};
class PlatformServicesTest final : public QObject {
    Q_OBJECT
private slots:
    void appearanceLeavesNativePlatformServicesWithTheirOwner() {
        auto services = std::make_unique<GenericServicesFixture>();
        auto *observed = services.get();
        QindaQt::QtTheme::PlatformTheme theme(std::move(services), {}, [] { return true; });
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
    // ADR-0130: the menubar decision is re-made at every creation from the
    // live probe, and an absent host never reaches the base theme (Qt's
    // generic theme would cache that first answer for the whole process).
    void menuBarIsDecidedPerCreationFromTheHostProbe() {
        auto services = std::make_unique<MenuBarServicesFixture>();
        auto *observed = services.get();
        bool hostPresent = false;
        int probes = 0;
        QindaQt::QtTheme::PlatformTheme theme(std::move(services), {}, [&] { ++probes; return hostPresent; });
        QCOMPARE(theme.createPlatformMenuBar(), nullptr);
        QCOMPARE(observed->menuBars, 0);
        hostPresent = true;
        std::unique_ptr<QPlatformMenuBar> hosted(theme.createPlatformMenuBar());
        QVERIFY(hosted != nullptr);
        QCOMPARE(observed->menuBars, 1);
        hostPresent = false;
        QCOMPARE(theme.createPlatformMenuBar(), nullptr);
        QCOMPARE(observed->menuBars, 1);
        hostPresent = true;
        std::unique_ptr<QPlatformMenuBar> restored(theme.createPlatformMenuBar());
        QVERIFY(restored != nullptr);
        QCOMPARE(observed->menuBars, 2);
        QCOMPARE(probes, 4);
        QindaQt::QtTheme::PlatformTheme noProbe(std::make_unique<MenuBarServicesFixture>(), {}, {});
        QCOMPARE(noProbe.createPlatformMenuBar(), nullptr);
    }
};
QTEST_MAIN(PlatformServicesTest)
#include "tst_platform_services.moc"
