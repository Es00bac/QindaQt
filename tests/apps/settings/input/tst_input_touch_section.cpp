// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_input/touch_settings_model.h>
#include <qindaqt/themes/theme_loader.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QTest>

#include "support/fake_settings_transport.h"

namespace QindaQt::Apps::SettingsInput {
using namespace QindaQt::Tests;
using Services::SettingsClient::SettingsClient;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {

QObject *findInTree(QObject *root, const QString &objectName)
{
    QList<QObject *> pending{root};
    QSet<QObject *> seen;
    while (!pending.isEmpty()) {
        QObject *node = pending.takeFirst();
        if (node == nullptr || seen.contains(node)) {
            continue;
        }
        seen.insert(node);
        if (node->objectName() == objectName) {
            return node;
        }
        pending.append(node->children());
        if (auto *item = qobject_cast<QQuickItem *>(node)) {
            const QList<QQuickItem *> childItems = item->childItems();
            for (QQuickItem *child : childItems) {
                pending.append(child);
            }
        }
    }
    return nullptr;
}

// The facade the section binds to: the real touch model over a fake transport.
class TouchFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *touch READ touch CONSTANT)
public:
    FakeSettingsTransport transport;
    SettingsClient client{transport, TouchSettingsModel::settingsKeys()};
    TouchSettingsModel model{client};
    QObject *touch() { return &model; }
    [[nodiscard]] bool deliverSnapshot(quint64 revision, const QVariantMap &values)
    {
        Q_EMIT transport.ownerChanged(QStringLiteral(":1.9"));
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 2000)) {
            return false;
        }
        Q_EMIT transport.snapshotReceived(transport.snapshots.constLast().token, QStringLiteral(":1.9"),
                                          fakeSnapshotWire(revision, withTouchDefaults(values)));
        return QTest::qWaitFor([this] { return model.available(); }, 2000);
    }
};

} // namespace

class InputTouchSectionTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void degradedUntilTheSnapshotArrivesThenRowsFollowIt();
    void editsGoThroughTheModelAsOneWrite();
    void aDisabledTouchscreenHidesTheOtherRows();
    void retainedValuesAreDisabledWhenAuthorityIsLost();
    void sliderKeyboardAndDragKeepTheFinalRequestedValue();
    void rejectedToggleRestoresAuthorityAndKeepsDiagnostic();

private:
    [[nodiscard]] QObject *findObject(const QString &objectName) const { return findInTree(section, objectName); }
    [[nodiscard]] bool isShown(const QString &objectName) const
    {
        const auto *item = qobject_cast<QQuickItem *>(findObject(objectName));
        return item != nullptr && item->isVisible();
    }

    std::unique_ptr<TouchFacade> facade;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QObject *section = nullptr;
};

void InputTouchSectionTest::init()
{
    facade = std::make_unique<TouchFacade>();
    engine = std::make_unique<QQmlEngine>();
    engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    QString facadeError;
    auto *tokens = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*engine, &facadeError);
    QVERIFY2(tokens != nullptr, qPrintable(facadeError));
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QString publishError;
    QVERIFY2(tokens->publish(theme.theme, {}, &publishError), qPrintable(publishError));
    QQmlComponent component(engine.get());
    component.setData(QByteArray("import QtQuick\n"
                                 "import QindaQt.SettingsApp.Input\n"
                                 "InputTouchSection { objectName: \"inputTouchSection\" }\n"),
                      QUrl(QStringLiteral("qindaqt-test://input-touch-section")));
    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 10000);
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    section = component.createWithInitialProperties(
        {{QStringLiteral("inputSettings"), QVariant::fromValue(static_cast<QObject *>(facade.get()))}});
    auto *item = qobject_cast<QQuickItem *>(section);
    QVERIFY2(item != nullptr, qPrintable(component.errorString()));
    window = std::make_unique<QQuickWindow>();
    window->resize(900, 700);
    item->setParentItem(window->contentItem());
    item->setWidth(900);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
}

void InputTouchSectionTest::cleanup()
{
    window.reset();
    engine.reset();
    facade.reset();
    section = nullptr;
}

void InputTouchSectionTest::degradedUntilTheSnapshotArrivesThenRowsFollowIt()
{
    // Component.onCompleted refreshed the model: the client started and asked
    // for its scoped snapshot; until it answers the section is degraded.
    QCOMPARE(facade->transport.starts, 1);
    QVERIFY(isShown(QStringLiteral("inputTouchDegraded")));
    QVERIFY(!isShown(QStringLiteral("inputTouchEnabledRow")));
    QVERIFY(facade->deliverSnapshot(3, {{QStringLiteral("input.touch.longPressMs"), 800},
                                        {QStringLiteral("input.touch.edgeRight"), QStringLiteral("overview")}}));
    QTRY_VERIFY(!isShown(QStringLiteral("inputTouchDegraded")));
    QVERIFY(isShown(QStringLiteral("inputTouchEnabledRow")));
    QVERIFY(isShown(QStringLiteral("inputTouchLongPressRow")));
    // input.touch.mode has no consumer: no row offers it (ADR-0205 §4).
    QVERIFY(findObject(QStringLiteral("inputTouchModeRow")) == nullptr);
    QVERIFY(isShown(QStringLiteral("inputTouchKeyboardRow")));
    QCOMPARE(findObject(QStringLiteral("inputTouchEnabledSwitch"))->property("checked").toBool(), true);
    QCOMPARE(findObject(QStringLiteral("inputTouchLongPressSlider"))->property("value").toDouble(), 800.0);
    QTRY_COMPARE(findObject(QStringLiteral("inputTouchKeyboardCombo"))->property("currentIndex").toInt(), 0);
    for (const char *edge : {"left", "top", "right", "bottom"}) {
        QVERIFY2(isShown(QStringLiteral("inputTouchEdgeRow_") + QLatin1String(edge)), edge);
    }
    // ComboBox manages currentIndex internally, so an explicit binding can
    // settle a turn late when the delegate is (re)created; retry.
    QTRY_COMPARE(findObject(QStringLiteral("inputTouchEdgeCombo_right"))->property("currentIndex").toInt(), 1);
    QTRY_COMPARE(findObject(QStringLiteral("inputTouchEdgeCombo_bottom"))->property("currentIndex").toInt(), 3);
    QVERIFY(findObject(QStringLiteral("inputTouchStatus"))->property("text").toString().contains(QStringLiteral("800")));
}

void InputTouchSectionTest::editsGoThroughTheModelAsOneWrite()
{
    QVERIFY(facade->deliverSnapshot(1, {}));
    QTRY_VERIFY(isShown(QStringLiteral("inputTouchEnabledRow")));
    QObject *toggle = findObject(QStringLiteral("inputTouchEnabledSwitch"));
    QVERIFY(toggle != nullptr);
    // Activate the real Switch path so `toggled` fires as a user action would.
    QVERIFY(QMetaObject::invokeMethod(toggle, "toggle"));
    QMetaObject::invokeMethod(toggle, "toggled");
    QTRY_COMPARE(facade->transport.commits.size(), 1);
    const QVariantMap operation = facade->transport.commits.constFirst().operations.constFirst().toMap();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(), QStringLiteral("input.touch.enabled"));
    QVERIFY(facade->model.busy());
    QVERIFY(!findObject(QStringLiteral("inputTouchEnabledSwitch"))->property("enabled").toBool());
    Q_EMIT facade->transport.commitReceived(
        facade->transport.commits.constFirst().token, QStringLiteral(":1.9"),
        fakeCommitWire(SettingsWireStatus::Applied, 1, 2, {{QStringLiteral("input.touch.enabled"), false}}));
    QVERIFY(facade->model.busy());
    // The client re-reads its scope after the commit; the rows follow that
    // confirmed snapshot, not the reply.
    QTRY_VERIFY(facade->transport.snapshots.size() >= 2);
    Q_EMIT facade->transport.snapshotReceived(facade->transport.snapshots.constLast().token, QStringLiteral(":1.9"),
                                              fakeSnapshotWire(2, withTouchDefaults({{QStringLiteral("input.touch.enabled"), false}})));
    QTRY_VERIFY(!isShown(QStringLiteral("inputTouchLongPressRow")));
    QTRY_VERIFY(!facade->model.busy());
}

void InputTouchSectionTest::aDisabledTouchscreenHidesTheOtherRows()
{
    QVERIFY(facade->deliverSnapshot(2, {{QStringLiteral("input.touch.enabled"), false}}));
    QTRY_VERIFY(isShown(QStringLiteral("inputTouchEnabledRow")));
    QVERIFY(!isShown(QStringLiteral("inputTouchLongPressRow")));
    QVERIFY(!isShown(QStringLiteral("inputTouchEdgeRow_left")));
    QCOMPARE(findObject(QStringLiteral("inputTouchEnabledSwitch"))->property("checked").toBool(), false);
    QCOMPARE(findObject(QStringLiteral("inputTouchStatus"))->property("text").toString(),
             QStringLiteral("The touchscreen is off."));
}

void InputTouchSectionTest::retainedValuesAreDisabledWhenAuthorityIsLost()
{
    QVERIFY(facade->deliverSnapshot(1, {{QStringLiteral("input.touch.longPressMs"), 850}}));
    QTRY_VERIFY(isShown(QStringLiteral("inputTouchLongPressRow")));
    Q_EMIT facade->transport.ownerChanged(QString{});
    QTRY_VERIFY(isShown(QStringLiteral("inputTouchDegraded")));
    QVERIFY(isShown(QStringLiteral("inputTouchEnabledRow")));
    QCOMPARE(findObject(QStringLiteral("inputTouchLongPressSlider"))->property("value").toInt(), 850);
    for (const char *name : {"inputTouchEnabledSwitch", "inputTouchLongPressSlider",
                             "inputTouchKeyboardCombo", "inputTouchEdgeCombo_left"}) {
        QObject *control = findObject(QString::fromLatin1(name));
        QVERIFY2(control != nullptr, name);
        QVERIFY2(!control->property("enabled").toBool(), name);
    }
    QVERIFY(!facade->model.setLongPressMs(900));
    QVERIFY(facade->transport.commits.isEmpty());
}

void InputTouchSectionTest::sliderKeyboardAndDragKeepTheFinalRequestedValue()
{
    QVERIFY(facade->deliverSnapshot(1, {}));
    auto *slider = qobject_cast<QQuickItem *>(findObject(QStringLiteral("inputTouchLongPressSlider")));
    QVERIFY(slider != nullptr);
    QTRY_VERIFY(slider->isVisible());
    slider->forceActiveFocus();
    QVERIFY(slider->hasActiveFocus());
    QTest::keyClick(window.get(), Qt::Key_Right);
    QTRY_COMPARE(facade->transport.commits.size(), 1);
    QCOMPARE(facade->model.longPressMs(), 500);
    QCOMPARE(facade->model.longPressDisplayMs(), 550);
    QVERIFY(slider->isEnabled());
    QTest::keyClick(window.get(), Qt::Key_Right);
    QTRY_COMPARE(facade->model.longPressDisplayMs(), 600);
    QCOMPARE(facade->transport.commits.size(), 1);

    const QPointF fromScene = slider->mapToScene(QPointF(slider->width() * 0.31, slider->height() / 2.0));
    const QPointF toScene = slider->mapToScene(QPointF(slider->width() * 0.55, slider->height() / 2.0));
    QTest::mousePress(window.get(), Qt::LeftButton, Qt::NoModifier, fromScene.toPoint());
    QTest::mouseMove(window.get(), toScene.toPoint(), 15);
    QTest::mouseRelease(window.get(), Qt::LeftButton, Qt::NoModifier, toScene.toPoint());
    const int finalRequested = facade->model.longPressDisplayMs();
    QVERIFY(finalRequested > 600);
    QCOMPARE(facade->transport.commits.size(), 1);
    Q_EMIT facade->transport.commitReceived(
        facade->transport.commits.constFirst().token, QStringLiteral(":1.9"),
        fakeCommitWire(SettingsWireStatus::Applied, 1, 2,
                       {{QStringLiteral("input.touch.longPressMs"), 550}}));
    QCOMPARE(facade->transport.commits.size(), 1);
    QTRY_VERIFY(facade->transport.snapshots.size() >= 2);
    Q_EMIT facade->transport.snapshotReceived(
        facade->transport.snapshots.constLast().token, QStringLiteral(":1.9"),
        fakeSnapshotWire(2, withTouchDefaults({{QStringLiteral("input.touch.longPressMs"), 550}})));
    QTRY_COMPARE(facade->transport.commits.size(), 2);
    QCOMPARE(facade->transport.commits.constLast().operations.constFirst().toMap()
                 .value(QStringLiteral("value")).toInt(), finalRequested);
    Q_EMIT facade->transport.commitReceived(
        facade->transport.commits.constLast().token, QStringLiteral(":1.9"),
        fakeCommitWire(SettingsWireStatus::ValidationFailed, 2, 2,
                       {{QStringLiteral("input.touch.longPressMs"), 550}}));
    QTRY_VERIFY(!facade->model.busy());
    QVERIFY(!facade->model.errorText().isEmpty());
    QTRY_COMPARE(slider->property("value").toInt(), 550);
}

void InputTouchSectionTest::rejectedToggleRestoresAuthorityAndKeepsDiagnostic()
{
    QVERIFY(facade->deliverSnapshot(1, {}));
    auto *toggle = qobject_cast<QQuickItem *>(findObject(QStringLiteral("inputTouchEnabledSwitch")));
    QVERIFY(toggle != nullptr);
    QTRY_VERIFY(toggle->isVisible());
    // A snapshot changes the ColumnLayout's visible children. Wait for its
    // polish before sending a pointer event: before polish the status label
    // temporarily overlaps the row and wins the hit test.
    QTRY_VERIFY(toggle->parentItem()->width() >= 600);
    const QPoint scene = toggle->mapToScene(QPointF(toggle->width() / 2.0,
                                                     toggle->height() / 2.0)).toPoint();
    QTest::mouseClick(window.get(), Qt::LeftButton, Qt::NoModifier, scene);
    QTRY_COMPARE(facade->transport.commits.size(), 1);
    QVERIFY(facade->model.busy());
    QVERIFY(!toggle->isEnabled());
    QCOMPARE(toggle->property("checked").toBool(), true);
    Q_EMIT facade->transport.commitReceived(
        facade->transport.commits.constFirst().token, QStringLiteral(":1.9"),
        fakeCommitWire(SettingsWireStatus::ValidationFailed, 1, 1,
                       {{QStringLiteral("input.touch.enabled"), true}}));
    QTRY_VERIFY(!facade->model.busy());
    QCOMPARE(toggle->property("checked").toBool(), true);
    QVERIFY(!facade->model.errorText().isEmpty());
    QTRY_VERIFY(facade->transport.snapshots.size() >= 2);
    Q_EMIT facade->transport.snapshotReceived(
        facade->transport.snapshots.constLast().token, QStringLiteral(":1.9"),
        fakeSnapshotWire(1, withTouchDefaults({})));
    QTRY_VERIFY(facade->model.available());
    QCOMPARE(toggle->property("checked").toBool(), true);
    QVERIFY(!facade->model.errorText().isEmpty());
    QCOMPARE(facade->transport.commits.size(), 1);
}

} // namespace QindaQt::Apps::SettingsInput

QTEST_MAIN(QindaQt::Apps::SettingsInput::InputTouchSectionTest)
#include "tst_input_touch_section.moc"
