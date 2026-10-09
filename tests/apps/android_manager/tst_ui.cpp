// SPDX-License-Identifier: GPL-3.0-or-later
#include "manager_test_support.h"
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QDir>
#include <QImage>
using namespace QindaQt::AndroidManager;
using namespace QindaQt::AndroidManager::TestSupport;

namespace {
QQuickItem *visualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = visualItem(child, name)) return found;
    return nullptr;
}
void load(QQmlApplicationEngine &engine, ApplicationController &controller)
{
    engine.setInitialProperties({{QStringLiteral("manager"), QVariant::fromValue(&controller)}});
    engine.load(QUrl::fromLocalFile(QStringLiteral(ANDROID_MANAGER_MAIN_QML)));
}
}
class AndroidManagerUiTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void setupSelectionIsActionableWithoutFakeRegistration();
    void unavailableWindowIsHonest();
    void registeredRowsExposeOriginAndFencedLaunch();
    void ownerLossClearsTheRows();
};
void AndroidManagerUiTest::setupSelectionIsActionableWithoutFakeRegistration()
{
    FakeSetupPort setup;
    ApplicationController controller(nullptr, nullptr, 30000, &setup);
    QQmlApplicationEngine engine;
    load(engine, controller);
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QTest::qWait(50);
    auto *button = window->findChild<QQuickItem *>(QStringLiteral("androidRuntimeSetup"));
    QVERIFY(button && button->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                     button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
    QCOMPARE(setup.selections, (QVector<SetupTarget>{SetupTarget::Runtime}));
    QCOMPARE(controller.runtimeState(), QStringLiteral("unavailable"));
    QVERIFY(controller.applications().isEmpty());
    auto *message = window->findChild<QObject *>(QStringLiteral("androidSetupMessage"));
    QVERIFY(message && !message->property("text").toString().isEmpty());
    setup.setAvailable(false);
    QTRY_VERIFY_WITH_TIMEOUT(!button->isEnabled(), 500);
}

void AndroidManagerUiTest::unavailableWindowIsHonest()
{
    ApplicationController controller;
    QQmlApplicationEngine engine;
    load(engine, controller);
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QTest::qWait(25);
    auto *refresh = window->findChild<QObject *>(QStringLiteral("androidRefresh"));
    QVERIFY(refresh);
    QVERIFY(!refresh->property("enabled").toBool());
    QCOMPARE(window->title(), QStringLiteral("Android Applications"));
    auto *message = window->findChild<QObject *>(QStringLiteral("androidRuntimeMessage"));
    QVERIFY(message->property("text").toString().contains(QStringLiteral("unavailable")));
}
void AndroidManagerUiTest::registeredRowsExposeOriginAndFencedLaunch()
{
    FakePort port;
    ApplicationController controller(&port);
    QQmlApplicationEngine engine;
    load(engine, controller);
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    window->setWidth(420);
    QTest::qWait(50);
    auto *list = window->findChild<QQuickItem *>(QStringLiteral("androidApplicationList"));
    QVERIFY(list);
    QCOMPARE(list->property("count").toInt(), 2);
    QVERIFY(list->width() > 0 && list->height() > 0);
    const auto id = entryId(port.current);
    auto *badge = visualItem(window->contentItem(), QStringLiteral("androidOrigin-") + id);
    QVERIFY(badge);
    QCOMPARE(badge->property("text").toString(), QStringLiteral("Android"));
    auto *button = visualItem(window->contentItem(), QStringLiteral("androidLaunch-") + id);
    QVERIFY(button);
    QVERIFY(button->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                     button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
    QCOMPARE(port.requests.size(), 1);
    QCOMPARE(port.requests.first().application, port.current.registrations.first().binding);
    // A whole copied snapshot rebuilds ListView delegates at the next polish.
    // Observe the fresh visual item; the old QObject is no longer authority.
    QTRY_VERIFY_WITH_TIMEOUT(visualItem(window->contentItem(), QStringLiteral("androidLaunch-") + id)
        && !visualItem(window->contentItem(), QStringLiteral("androidLaunch-") + id)->isEnabled(), 500);
    QDir().mkpath(QStringLiteral(ANDROID_MANAGER_ARTIFACTS));
    QVERIFY(window->grabWindow().save(QStringLiteral(ANDROID_MANAGER_ARTIFACTS) + QStringLiteral("/synthetic-android-manager-420.png")));
}
void AndroidManagerUiTest::ownerLossClearsTheRows()
{
    FakePort port;
    ApplicationController controller(&port);
    QQmlApplicationEngine engine;
    load(engine, controller);
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QTest::qWait(25);
    port.publish({});
    QTest::qWait(25);
    auto *list = window->findChild<QObject *>(QStringLiteral("androidApplicationList"));
    QVERIFY(list);
    QCOMPARE(list->property("count").toInt(), 0);
}
QTEST_MAIN(AndroidManagerUiTest)
#include "tst_ui.moc"
