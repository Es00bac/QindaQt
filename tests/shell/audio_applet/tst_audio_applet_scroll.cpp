// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/audio_applet_qml_fixture.h"

#include <QImage>
#include <QGuiApplication>
#include <QScreen>
#include <QWheelEvent>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AudioAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

class SettingsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canOpenSettings READ canOpenSettings CONSTANT)
public:
    bool canOpenSettings() const { return true; }
    Q_INVOKABLE bool openSettings() { ++calls; return true; }
    int calls = 0;
};

namespace {
Audio::Snapshot manyRows()
{
    auto snapshot = clientSnapshot();
    const auto output = snapshot.outputs.constFirst();
    const auto input = snapshot.inputs.constFirst();
    const auto stream = snapshot.streams.constFirst();
    snapshot.outputs.clear();
    snapshot.inputs.clear();
    snapshot.streams.clear();
    for (quint64 i = 0; i < 8; ++i) {
        auto row = output;
        row.handle.serial = 10 + i;
        row.name = QStringLiteral("Output %1").arg(i);
        row.isDefault = i == 0;
        snapshot.outputs.append(row);
    }
    for (quint64 i = 0; i < 5; ++i) {
        auto row = input;
        row.handle.serial = 20 + i;
        row.name = QStringLiteral("Input %1").arg(i);
        row.isDefault = i == 0;
        snapshot.inputs.append(row);
    }
    for (quint64 i = 0; i < 24; ++i) {
        auto row = stream;
        row.handle.serial = 30 + i;
        row.applicationName = QStringLiteral("Application %1").arg(i);
        snapshot.streams.append(row);
    }
    snapshot.capabilities |= Audio::Capability::Console | Audio::Capability::SetConsoleGain;
    for (quint32 i = 0; i < 8; ++i) {
        Audio::Strip strip;
        strip.id = QStringLiteral("strip%1").arg(i);
        strip.label = QStringLiteral("Console %1").arg(i);
        strip.kind = i < 4 ? Audio::StripKind::HardwareInput : Audio::StripKind::VirtualInput;
        strip.index = i % 4;
        strip.gainDb = 0.0;
        snapshot.console.strips.append(strip);
    }
    return snapshot;
}

void wheelOver(QQuickItem *viewport)
{
    auto *window = viewport->window();
    const auto point = viewport->mapToScene(QPointF(4, 12));
    QWheelEvent event(point, window->mapToGlobal(point.toPoint()), {},
        QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &event);
}
} // namespace

class AudioAppletScrollTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constrainedPopupReachesFooterWithoutChangingAudio();
    void manyOutputsKeepTheDefaultInputAndOutput();
    void selectedOutputGeometryUsesCurrentWindowScreen();
    void composedSettingsFacadeIsOptionalAndInvoked();
};

void AudioAppletScrollTests::constrainedPopupReachesFooterWithoutChangingAudio()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), manyRows());
    QCOMPARE(client.state(), Audio::ClientState::Ready);
    QCOMPARE(countPendingRows(controller), 0);
    // A local refused request exposes the actual focusable footer Dismiss;
    // no service operation is issued and no host volume changes.
    QVERIFY(!controller.requestVolume(999, false, 0.5));

    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller, {QStringLiteral("audio-volume-medium")}, &error),
        qPrintable(error));
    auto *root = harness.root();
    QVERIFY(root);
    QQuickWindow window;
    const auto available = window.screen()->availableGeometry();
    window.setGeometry(available.topLeft().x(), available.topLeft().y(),
        qMin(1280, available.width()), qMin(720, available.height()));
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    root->setSize(QSizeF(32, 28));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const auto summaries = visualItemsNamed(root, QStringLiteral("audioAppletSummary"));
    QCOMPARE(summaries.size(), 1);
    summaries.constFirst()->forceActiveFocus();
    auto *content = openPopupContent(root, &window);
    QVERIFY(content);
    auto *popup = root->findChild<QObject *>(QStringLiteral("audioAppletPopup"));
    QVERIFY(popup);
    auto *popupWindow = content->window();
    QVERIFY(popupWindow);
    QVERIFY(QTest::qWaitForWindowExposed(popupWindow));
    // Old fixed620 popup fails on scaled constrained output and also fails
    // the compact480 budget; this assertion does not infer scrolling failure.
    const auto outputSpace = popup->property("outputSpace").toSize();
    qInfo() << "selected-output" << window.screen()->name()
            << available.size() << "popup-space" << outputSpace
            << "popup-height" << popup->property("height").toDouble();
    QCOMPARE(outputSpace, available.size());
    QVERIFY(popup->property("height").toDouble() <= qMin(480, available.height() - 28 - 16));
    QVERIFY(popup->property("width").toDouble() <= qMin(360, available.width() - 16));
    QVERIFY(content->clip());
    auto *viewport = content;
    if (!viewport->property("contentY").isValid())
        viewport = content->property("contentItem").value<QQuickItem *>();
    QVERIFY(viewport);
    QTRY_VERIFY(viewport->property("contentHeight").toDouble() > viewport->height());
    const auto footers = visualItemsNamed(viewport, QStringLiteral("audioAppletFooter"));
    QCOMPARE(footers.size(), 1);
    auto *footer = footers.constFirst();
    QVERIFY(footer->mapToItem(viewport, 0, 0).y() >= viewport->height());

    wheelOver(viewport);
    QTRY_VERIFY(viewport->property("contentY").toDouble() > 0);
    QCOMPARE(transport.operations.size(), 0);
    const auto bands = visualItemsNamed(viewport, QStringLiteral("audioOutputBand"));
    QCOMPARE(bands.size(), 1);
    bands.constFirst()->forceActiveFocus();
    QTest::keyClick(popupWindow, Qt::Key_End, Qt::ControlModifier);
    QTRY_VERIFY(footer->mapToItem(viewport, 0, 0).y() + footer->height() <= viewport->height() + 1);
    QTest::keyClick(popupWindow, Qt::Key_Home, Qt::ControlModifier);
    QTRY_COMPARE(viewport->property("contentY").toDouble(), 0.0);
    QTest::keyClick(popupWindow, Qt::Key_PageDown);
    QTRY_VERIFY(viewport->property("contentY").toDouble() > 0);
    QTest::keyClick(popupWindow, Qt::Key_PageUp);
    QTRY_COMPARE(viewport->property("contentY").toDouble(), 0.0);

    const auto dismisses = visualItemsNamed(viewport, QStringLiteral("audioStatusDismiss"));
    QCOMPARE(dismisses.size(), 1);
    auto *dismiss = dismisses.constFirst();
    // Actual Tab delivery must traverse to the last action and reveal it.
    bands.constFirst()->forceActiveFocus();
    for (int i = 0; i < 100 && !dismiss->hasActiveFocus(); ++i)
        QTest::keyClick(popupWindow, Qt::Key_Tab);
    QVERIFY(dismiss->hasActiveFocus());
    QTRY_VERIFY(dismiss->mapToItem(viewport, 0, 0).y() >= 0);
    QVERIFY(dismiss->mapToItem(viewport, 0, 0).y() + dismiss->height() <= viewport->height() + 1);
    QCOMPARE(transport.operations.size(), 0);
    const auto capturePath = qEnvironmentVariable("QINDAQT_AUDIO_CAPTURE_PATH");
    if (!capturePath.isEmpty()) {
        const auto capture = popupWindow->grabWindow();
        QVERIFY(!capture.isNull());
        QVERIFY(capture.save(capturePath));
    }
    // Re-resolve the actual window selection, including equal-size/name peers.
    for (auto *screen : QGuiApplication::screens()) {
        if (screen == window.screen())
            continue;
        window.setScreen(screen);
        QCOMPARE(window.screen(), screen);
        QTRY_COMPARE(popup->property("outputSpace").toSize(),
                     screen->availableGeometry().size());
        break;
    }

}
void AudioAppletScrollTests::manyOutputsKeepTheDefaultInputAndOutput()
{
    auto snapshot = manyRows();
    for (quint64 i = 0; i < 4; ++i) {
        auto row = snapshot.outputs.constFirst();
        row.handle.serial = 100 + i;
        row.isDefault = i == 3;
        snapshot.outputs.append(row);
    }
    snapshot.outputs[0].isDefault = false;
    snapshot.defaultOutput = snapshot.outputs.constLast().handle;
    snapshot.inputs[0].isDefault = false;
    snapshot.inputs.last().isDefault = true;
    snapshot.defaultInput = snapshot.inputs.constLast().handle;
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), snapshot);
    QCOMPARE(client.state(), Audio::ClientState::Ready);
    const auto rows = controller.deviceRows();
    QVERIFY(rows.size() <= 16);
    bool hasInput = false;
    bool hasOutput = false;
    for (const auto &value : rows) {
        const auto row = value.value<DeviceRow>();
        hasInput |= !row.isOutput() && row.isDefault() && row.serial() == 24;
        hasOutput |= row.isOutput() && row.isDefault() && row.serial() == 103;
    }
    QVERIFY(hasInput);
    QVERIFY(hasOutput);
}
void AudioAppletScrollTests::selectedOutputGeometryUsesCurrentWindowScreen()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    QQuickWindow window;
    QQuickItem anchor(window.contentItem());
    QVERIFY(controller.popupAvailableSize(nullptr, window.screen()->name()).isEmpty());
    QCOMPARE(controller.popupAvailableSize(&anchor, QString{}),
             window.screen()->availableGeometry().size());
    QQuickItem detachedAnchor;
    QVERIFY(controller.popupAvailableSize(&detachedAnchor, QString{}).isEmpty());
    QVERIFY(controller.popupAvailableSize(&anchor, QStringLiteral("missing-output")).isEmpty());
    QCOMPARE(controller.popupAvailableSize(&anchor, window.screen()->name()),
             window.screen()->availableGeometry().size());
    for (auto *screen : QGuiApplication::screens()) {
        if (screen == window.screen())
            continue;
        window.setScreen(screen);
        window.setGeometry(screen->availableGeometry());
        QCOMPARE(controller.popupAvailableSize(&anchor, screen->name()),
                 screen->availableGeometry().size());
    }
}

void AudioAppletScrollTests::composedSettingsFacadeIsOptionalAndInvoked()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), clientSnapshot());
    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller, {QStringLiteral("audio-volume-medium")}, &error),
        qPrintable(error));
    auto *root = harness.root();
    QQuickWindow window;
    window.setGeometry(0, 0, 640, 360);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    root->setSize(QSizeF(32, 28));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const auto summaries = visualItemsNamed(root, QStringLiteral("audioAppletSummary"));
    QCOMPARE(summaries.size(), 1);
    summaries.constFirst()->forceActiveFocus();
    auto *content = openPopupContent(root, &window);
    QVERIFY(content);
    auto settings = visualItemsNamed(content, QStringLiteral("audioOpenSettings"));
    QCOMPARE(settings.size(), 1);
    QVERIFY(!settings.constFirst()->isVisible());
    SettingsFacade facade;
    QVERIFY(root->setProperty("desktopControls", QVariant::fromValue<QObject *>(&facade)));
    QTRY_VERIFY(settings.constFirst()->isVisible());
    auto *button = settings.constFirst();
    button->forceActiveFocus();
    QTest::keyClick(button->window(), Qt::Key_Space);
    QTRY_COMPARE(facade.calls, 1);
    QCOMPARE(transport.operations.size(), 0);
    QVERIFY(root->setProperty("desktopControls", QVariant::fromValue<QObject *>(nullptr)));
    QTRY_VERIFY(!button->isVisible());
}
QTEST_MAIN(AudioAppletScrollTests)
#include "tst_audio_applet_scroll.moc"
