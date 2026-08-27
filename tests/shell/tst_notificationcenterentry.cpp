// SPDX-License-Identifier: GPL-3.0-or-later
#include "globalshortcutregistrar.h"
#include "notificationcenterappletaccess.h"
#include "notificationcentershortcut.h"

#include <QAction>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell;

namespace {

class FakeRegistrar final : public GlobalShortcutRegistrar {
public:
    GlobalShortcutRegistration registerShortcut(
        QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
        std::function<void(bool)> activeBindingChanged) override
    {
        observedAction = &action;
        observedDefault = defaultShortcut;
        observedLifetime = &lifetime;
        changed = std::move(activeBindingChanged);
        return result;
    }

    GlobalShortcutRegistration result{true, false};
    QAction *observedAction = nullptr;
    QKeySequence observedDefault;
    QObject *observedLifetime = nullptr;
    std::function<void(bool)> changed;
};

} // namespace

class NotificationCenterEntryTests final : public QObject {
    Q_OBJECT

private slots:
    void facadePublishesOnlyToggleAndOpenState();
    void shortcutExposesStableIdentityAndTracksRegistration();
    void shortcutKeepsRegistrationRequestSeparateFromActiveBinding_data();
    void shortcutKeepsRegistrationRequestSeparateFromActiveBinding();
};

void NotificationCenterEntryTests::facadePublishesOnlyToggleAndOpenState()
{
    NotificationCenterAppletAccess access;
    QSignalSpy toggles(&access, &NotificationCenterAppletAccess::toggleRequested);
    QSignalSpy stateChanges(&access,
                            &NotificationCenterAppletAccess::centerOpenChanged);

    QVERIFY(!access.centerOpen());
    QVERIFY(!access.doNotDisturbEnabled());
    const int centerOpenProperty =
        access.metaObject()->indexOfProperty("centerOpen");
    const int dndProperty =
        access.metaObject()->indexOfProperty("doNotDisturbEnabled");
    QVERIFY(centerOpenProperty >= 0);
    QVERIFY(dndProperty >= 0);
    QVERIFY(!access.metaObject()->property(centerOpenProperty).isWritable());
    QVERIFY(!access.metaObject()->property(dndProperty).isWritable());
    QVERIFY(access.metaObject()->indexOfMethod("toggle()") >= 0);
    QCOMPARE(access.metaObject()->indexOfMethod("publishCenterOpen(bool)"), -1);
    QCOMPARE(access.metaObject()->indexOfMethod(
                 "publishDoNotDisturbEnabled(bool)"),
             -1);
    QCOMPARE(access.metaObject()->indexOfMethod(
                 "setDoNotDisturbEnabled(bool)"),
             -1);
    access.toggle();
    QCOMPARE(toggles.size(), 1);
    QVERIFY(!access.centerOpen());

    access.publishCenterOpen(true);
    QVERIFY(access.centerOpen());
    QCOMPARE(stateChanges.size(), 1);
    access.publishCenterOpen(true);
    QCOMPARE(stateChanges.size(), 1);
    access.publishCenterOpen(false);
    QCOMPARE(stateChanges.size(), 2);

    QSignalSpy dndChanges(
        &access, &NotificationCenterAppletAccess::doNotDisturbEnabledChanged);
    access.publishDoNotDisturbEnabled(true);
    QVERIFY(access.doNotDisturbEnabled());
    QCOMPARE(dndChanges.size(), 1);
    access.publishDoNotDisturbEnabled(true);
    QCOMPARE(dndChanges.size(), 1);
    access.publishDoNotDisturbEnabled(false);
    QVERIFY(!access.doNotDisturbEnabled());
    QCOMPARE(dndChanges.size(), 2);
}

void NotificationCenterEntryTests::
    shortcutExposesStableIdentityAndTracksRegistration()
{
    FakeRegistrar registrar;
    int toggleCount = 0;
    NotificationCenterShortcut shortcut(registrar, [&] { ++toggleCount; });

    QCOMPARE(shortcut.action(), registrar.observedAction);
    QCOMPARE(registrar.observedLifetime, &shortcut);
    QCOMPARE(shortcut.action()->objectName(),
             QStringLiteral("qindaqt_toggle_notification_center"));
    QCOMPARE(NotificationCenterShortcut::stableActionId(),
             shortcut.action()->objectName());
    QCOMPARE(NotificationCenterShortcut::defaultShortcut(),
             QKeySequence(Qt::META | Qt::Key_N));
    QCOMPARE(registrar.observedDefault,
             NotificationCenterShortcut::defaultShortcut());
    QVERIFY(shortcut.registrationRequestAccepted());
    QVERIFY(!shortcut.activeBindingPresent());

    shortcut.action()->trigger();
    QCOMPARE(toggleCount, 1);

    QSignalSpy bindingChanges(
        &shortcut, &NotificationCenterShortcut::activeBindingPresentChanged);
    registrar.changed(true);
    QVERIFY(shortcut.activeBindingPresent());
    QCOMPARE(bindingChanges.size(), 1);
    registrar.changed(true);
    QCOMPARE(bindingChanges.size(), 1);
    registrar.changed(false);
    QVERIFY(!shortcut.activeBindingPresent());
    QCOMPARE(bindingChanges.size(), 2);
}

void NotificationCenterEntryTests::
    shortcutKeepsRegistrationRequestSeparateFromActiveBinding_data()
{
    QTest::addColumn<bool>("requestAccepted");
    QTest::addColumn<bool>("activeBindingPresent");

    QTest::newRow("request-rejected-and-no-binding") << false << false;
    QTest::newRow("request-accepted-and-binding-present") << true << true;
    QTest::newRow("request-rejected-but-existing-binding-observed")
        << false << true;
}

void NotificationCenterEntryTests::
    shortcutKeepsRegistrationRequestSeparateFromActiveBinding()
{
    QFETCH(bool, requestAccepted);
    QFETCH(bool, activeBindingPresent);

    FakeRegistrar registrar;
    registrar.result = {requestAccepted, activeBindingPresent};
    NotificationCenterShortcut shortcut(registrar, [] {});

    QCOMPARE(shortcut.registrationRequestAccepted(), requestAccepted);
    QCOMPARE(shortcut.activeBindingPresent(), activeBindingPresent);
}

QTEST_MAIN(NotificationCenterEntryTests)

#include "tst_notificationcenterentry.moc"
