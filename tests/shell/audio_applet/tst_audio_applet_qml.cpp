// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_applet_controller.h"
#include "../icon_resolution_test_fixture.h"

#include "support/fake_audio_transport.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QPointer>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AudioAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::AudioApplet;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root == nullptr)
        return matches;
    if (root->objectName() == name)
        matches.append(root);
    for (QQuickItem *child : root->childItems())
        matches.append(visualItemsNamed(child, name));
    return matches;
}

int countPendingRows(const AudioAppletController &controller)
{
    int pending = 0;
    const QVariantList devices = controller.deviceRows();
    for (const QVariant &value : devices) {
        if (value.value<DeviceRow>().pending()) {
            ++pending;
        }
    }
    const QVariantList streams = controller.streamRows();
    for (const QVariant &value : streams) {
        if (value.value<StreamRow>().pending()) {
            ++pending;
        }
    }
    return pending;
}

[[nodiscard]] DeviceRow firstDeviceRow(const AudioAppletController &controller)
{
    const QVariantList rows = controller.deviceRows();
    return rows.isEmpty() ? DeviceRow{} : rows.constFirst().value<DeviceRow>();
}

// The compiled applet needs an import path, resolved icons, and a published
// theme before it can be instantiated, and every row in this file needs
// exactly that.
struct AppletHarness {
    QQmlEngine engine;
    std::unique_ptr<QObject> tokenRegistration;
    std::unique_ptr<QObject> applet;

    [[nodiscard]] QQuickItem *root() const
    {
        return qobject_cast<QQuickItem *>(applet.get());
    }
};

[[nodiscard]] bool loadApplet(AppletHarness &harness,
                              AudioAppletController *controller,
                              const QStringList &iconNames, QString *error)
{
    harness.engine.addImportPath(
        QStringLiteral(QINDAQT_AUDIO_APPLET_QML_IMPORT_PATH));
    if (!Tests::installResolvedIconFixture(
            harness.engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
            iconNames, error)) {
        return false;
    }

    // AGENT-NOTE: QindaQt.Controls resolves QST-1 roles from the read-only
    // Tokens singleton; without a published theme the state cards render
    // undefined tokens. Publication is the same seam production composition
    // uses, exercised here through the generated plugin path.
    QQmlComponent registration(&harness.engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    if (!QTest::qWaitFor([&registration] {
            return registration.status() != QQmlComponent::Loading;
        })) {
        *error = QStringLiteral("timed out loading the QindaQt.Tokens module");
        return false;
    }
    if (!registration.isReady()) {
        *error = registration.errorString();
        return false;
    }
    harness.tokenRegistration.reset(registration.create());
    if (harness.tokenRegistration == nullptr) {
        *error = QStringLiteral("the QindaQt.Tokens registration failed");
        return false;
    }

    auto *facade = harness.engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (facade == nullptr) {
        *error = QStringLiteral("the Tokens singleton did not resolve");
        return false;
    }
    const auto loaded = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!loaded.ok) {
        *error = loaded.error;
        return false;
    }
    if (!facade->publish(loaded.theme, {}, error)) {
        return false;
    }

    QQmlComponent component(&harness.engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.AudioApplet"),
                             QStringLiteral("AudioApplet"));
    if (!component.isReady()) {
        *error = component.errorString();
        return false;
    }
    harness.applet.reset(component.createWithInitialProperties(
        {{QStringLiteral("controller"), QVariant::fromValue(controller)}}));
    if (harness.applet == nullptr) {
        *error = component.errorString();
        return false;
    }
    return true;
}

// Returns the opened details popup's content item; all device and stream
// controls live inside it.
[[nodiscard]] QQuickItem *openPopupContent(QQuickItem *root,
                                           QQuickWindow *window)
{
    auto *popup = root->findChild<QObject *>(
        QStringLiteral("audioAppletPopup"));
    if (popup == nullptr)
        return nullptr;
    QTest::keyClick(window, Qt::Key_Return);
    if (!QTest::qWaitFor([popup] {
            return popup->property("opened").toBool();
        })) {
        return nullptr;
    }
    return popup->property("contentItem").value<QQuickItem *>();
}

} // namespace

class AudioAppletQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compiledAppletSupportsKeyboardAndAccessibility();
    void aDragKeepsTheHandleAndSendsTheLatestValue();
    void aPointerDragRidesTheSliderAndSendsWhereItStopped();
    void summaryIconTracksDefaultOutput_data();
    void summaryIconTracksDefaultOutput();
};

void AudioAppletQmlTests::compiledAppletSupportsKeyboardAndAccessibility()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), clientSnapshot());
    QCOMPARE(client.state(), Audio::ClientState::Ready);

    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller,
                        {QStringLiteral("audio-volume-medium")}, &error),
             qPrintable(error));
    QQuickItem *root = harness.root();
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 520);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    window.show();
    QTRY_VERIFY(window.isExposed());

    QCOMPARE(root->objectName(), QStringLiteral("audioApplet"));
    QAccessibleInterface *rootInterface = QAccessible::queryAccessibleInterface(root);
    QVERIFY(rootInterface != nullptr);
    QCOMPARE(rootInterface->role(), QAccessible::Grouping);
    QCOMPARE(rootInterface->text(QAccessible::Name),
             QStringLiteral("Audio"));
    QVERIFY(!rootInterface->text(QAccessible::Description).isEmpty());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("audioAppletSummary"));
    QVERIFY(summary != nullptr);
    QCOMPARE(root->width(), 32.0);
    QCOMPARE(root->height(), 28.0);
    QCOMPARE(summary->property("text").toString(), QString());
    auto *summaryIcon = summary->findChild<QQuickItem *>(
        QStringLiteral("audioAppletIcon"));
    QVERIFY(summaryIcon != nullptr);
    QVERIFY(Tests::hasResolvedProviderSource(
        summaryIcon, QStringLiteral("audio-volume-medium")));

    summary->forceActiveFocus();
    QQuickItem *popupContent = openPopupContent(root, &window);
    QVERIFY(popupContent != nullptr);

    // One slider per device with known volume (two outputs — physical and a
    // managed virtual device — plus one input) and one stream slider; every
    // control carries a complete accessible identity.
    const auto deviceSliders = visualItemsNamed(
        popupContent, QStringLiteral("audioDeviceVolume"));
    QCOMPARE(deviceSliders.size(), 3);
    const auto streamSliders = visualItemsNamed(
        popupContent, QStringLiteral("audioStreamVolume"));
    QCOMPARE(streamSliders.size(), 1);

    QQuickItem *slider = deviceSliders.constFirst();
    QAccessibleInterface *sliderInterface =
        QAccessible::queryAccessibleInterface(slider);
    QVERIFY(sliderInterface != nullptr);
    QCOMPARE(sliderInterface->role(), QAccessible::Slider);
    QVERIFY(sliderInterface->text(QAccessible::Name).contains(
        QStringLiteral("Output")));
    QVERIFY(!sliderInterface->text(QAccessible::Description).isEmpty());
    QVERIFY(slider->isEnabled());

    // A keyboard step dispatches exactly one clamped public-client operation
    // and marks the row pending until the exactly-once completion arrives.
    slider->forceActiveFocus();
    QVERIFY(slider->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Right);
    QTRY_COMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.kind,
             Audio::OperationKind::SetVolume);
    QCOMPARE(transport.operations.constFirst().request.primary.serial, 10ULL);
    // ADR-0191: the keyboard step is 1 %, not 5 %. A 5 % arrow step on a
    // 0-to-1 slider is a coarse control, and the same stepSize governed the
    // pointer drag that could only ever take one step.
    QCOMPARE(transport.operations.constFirst().request.volume, 0.51);
    QCOMPARE(countPendingRows(controller), 1);

    transport.finish(transport.operations.constFirst(),
                     successfulResult(transport.operations.constFirst(), 2));
    QTRY_COMPARE(countPendingRows(controller), 0);

    // The mute switch exposes an accessible role and description as well.
    const auto muteSwitches = visualItemsNamed(
        popupContent, QStringLiteral("audioDeviceMute"));
    QCOMPARE(muteSwitches.size(), 3);
    QAccessibleInterface *muteInterface =
        QAccessible::queryAccessibleInterface(muteSwitches.constFirst());
    QVERIFY(muteInterface != nullptr);
    QVERIFY(!muteInterface->text(QAccessible::Description).isEmpty());
}

// ADR-0191: a drag is many moves with one request in flight. The control stays
// adjustable, keeps the value the user chose for the whole round trip, and the
// latest value reaches the service when the in-flight request lands.
void AudioAppletQmlTests::aDragKeepsTheHandleAndSendsTheLatestValue()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), clientSnapshot());
    QCOMPARE(client.state(), Audio::ClientState::Ready);

    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller,
                        {QStringLiteral("audio-volume-medium")}, &error),
             qPrintable(error));
    QQuickItem *root = harness.root();
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 520);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    window.show();
    QTRY_VERIFY(window.isExposed());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("audioAppletSummary"));
    QVERIFY(summary != nullptr);
    summary->forceActiveFocus();
    QQuickItem *popupContent = openPopupContent(root, &window);
    QVERIFY(popupContent != nullptr);

    const auto sliders = visualItemsNamed(popupContent,
                                          QStringLiteral("audioDeviceVolume"));
    QVERIFY(!sliders.isEmpty());
    QQuickItem *slider = sliders.constFirst();
    QCOMPARE(slider->property("value").toDouble(), 0.5);
    slider->forceActiveFocus();
    QVERIFY(slider->hasActiveFocus());
    const QPointer<QQuickItem> tracked(slider);

    // One step dispatches, and the handle keeps the user's value for the whole
    // round trip. Rebinding to the snapshot the moment the key came up would
    // put the handle back at 0.50 until the service echoed the change, and the
    // next step would then ask for 0.51 again instead of advancing.
    QTest::keyClick(&window, Qt::Key_Right);
    QTRY_COMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.volume, 0.51);
    QCOMPARE(countPendingRows(controller), 1);

    // The reprojection the dispatch itself triggers must not destroy the
    // control: a recreated delegate loses the pointer grab and the keyboard
    // focus, which is what limited every drag to a single step.
    QVERIFY2(!tracked.isNull(),
             "the reprojection destroyed the control being used");
    QVERIFY(slider->hasActiveFocus());
    QCOMPARE(slider->property("value").toDouble(), 0.51);

    // The row is pending and still adjustable: a control that disabled itself
    // while its own request was in flight could not be moved past one step.
    QVERIFY(slider->isEnabled());

    // Two more steps with the first request still in flight. Neither is
    // dispatched, neither is refused, and the handle follows both.
    QTest::keyClick(&window, Qt::Key_Right);
    QTest::keyClick(&window, Qt::Key_Right);
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(slider->property("value").toDouble(), 0.53);

    // The completion drains the queue, sending the latest value and no
    // backlog of intermediate ones.
    transport.finish(transport.operations.constFirst(),
                     successfulResult(transport.operations.constFirst(), 3));
    QTRY_COMPARE(transport.operations.size(), 2);
    QCOMPARE(transport.operations.constLast().request.kind,
             Audio::OperationKind::SetVolume);
    QCOMPARE(transport.operations.constLast().request.volume, 0.53);

    // That last request succeeds and nothing is outstanding, but the service
    // has not published the new level yet. Handing the handle back to the
    // snapshot at this moment would drop it to 0.50 until the echo arrives.
    transport.finish(transport.operations.constLast(),
                     successfulResult(transport.operations.constLast(), 3));
    QTRY_COMPARE(countPendingRows(controller), 0);
    QVERIFY(firstDeviceRow(controller).volumeIsRequested());
    QCOMPARE(slider->property("value").toDouble(), 0.53);

    // The echo lands and the service owns the value again.
    Audio::Snapshot echoed = clientSnapshot(11, 4);
    echoed.outputs[0].volume = 0.53;
    transport.invalidate(kOwner, 11, 4);
    QVERIFY(!transport.fetches.isEmpty());
    transport.reply(transport.fetches.constLast(), echoed);
    QTRY_VERIFY(!firstDeviceRow(controller).volumeIsRequested());
    QCOMPARE(slider->property("value").toDouble(), 0.53);
}

// The reported symptom: dragging a volume slider moved it one step and then
// stopped. Every move during a press must keep the grab, and the service must
// end up with the value the finger stopped on.
void AudioAppletQmlTests::aPointerDragRidesTheSliderAndSendsWhereItStopped()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), clientSnapshot());
    QCOMPARE(client.state(), Audio::ClientState::Ready);

    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller,
                        {QStringLiteral("audio-volume-medium")}, &error),
             qPrintable(error));
    QQuickItem *root = harness.root();
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 640);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    window.show();
    QTRY_VERIFY(window.isExposed());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("audioAppletSummary"));
    QVERIFY(summary != nullptr);
    summary->forceActiveFocus();
    QQuickItem *popupContent = openPopupContent(root, &window);
    QVERIFY(popupContent != nullptr);

    const auto sliders = visualItemsNamed(popupContent,
                                          QStringLiteral("audioDeviceVolume"));
    QVERIFY(!sliders.isEmpty());
    QQuickItem *slider = sliders.constFirst();
    QVERIFY(slider->width() > 40.0);
    const QPointF centre = slider->mapToScene(
        QPointF(slider->width() / 2.0, slider->height() / 2.0));
    const QPointF right = slider->mapToScene(
        QPointF(slider->width() - 2.0, slider->height() / 2.0));
    const QPointF middleRight = slider->mapToScene(
        QPointF(slider->width() * 0.75, slider->height() / 2.0));

    // The details popup gets its own QQuickPopupWindow, so pointer events go
    // to the window the control actually lives in.
    QQuickWindow *sliderWindow = slider->window();
    QVERIFY(sliderWindow != nullptr);

    const QPointer<QQuickItem> tracked(slider);
    QTest::mousePress(sliderWindow, Qt::LeftButton, {}, centre.toPoint());
    QVERIFY(slider->property("pressed").toBool());

    // Three moves inside one press. The first dispatches; the rest coalesce
    // behind it. None of them may destroy the item under the pointer or take
    // the grab away from it.
    //
    // AGENT-NOTE: QTest::mouseMove synthesizes a move with no buttons held, so
    // `pressed` reads false for the duration here even though the grab and the
    // value tracking are intact. The grabber and the value are the product
    // behaviour; `pressed` mid-drag would only measure the harness.
    QTest::mouseMove(sliderWindow, middleRight.toPoint());
    QTRY_COMPARE(transport.operations.size(), 1);
    QVERIFY2(!tracked.isNull(), "the dispatch destroyed the dragged control");
    QCOMPARE(sliderWindow->mouseGrabberItem(), slider);
    const double afterFirstMove = slider->property("value").toDouble();
    QVERIFY(afterFirstMove > 0.5);

    QTest::mouseMove(sliderWindow, right.toPoint());
    QCOMPARE(slider->property("value").toDouble(), 1.0);
    QTest::mouseMove(sliderWindow, middleRight.toPoint());
    QVERIFY2(!tracked.isNull(), "a reprojection destroyed the dragged control");
    QCOMPARE(sliderWindow->mouseGrabberItem(), slider);
    QCOMPARE(slider->property("value").toDouble(), afterFirstMove);
    QCOMPARE(transport.operations.size(), 1);

    QTest::mouseRelease(sliderWindow, Qt::LeftButton, {}, middleRight.toPoint());
    const double released = slider->property("value").toDouble();
    QVERIFY(released > 0.5);

    // The handle keeps the value the finger left it on instead of snapping
    // back to the level the service still reports.
    QCOMPARE(countPendingRows(controller), 1);
    QCOMPARE(slider->property("value").toDouble(), released);

    // Completing the in-flight request sends exactly where the drag stopped.
    transport.finish(transport.operations.constFirst(),
                     successfulResult(transport.operations.constFirst(), 3));
    QTRY_COMPARE(transport.operations.size(), 2);
    QCOMPARE(transport.operations.constLast().request.volume, released);
    QCOMPARE(slider->property("value").toDouble(), released);
}

void AudioAppletQmlTests::summaryIconTracksDefaultOutput_data()
{
    QTest::addColumn<double>("volume");
    QTest::addColumn<bool>("muted");
    QTest::addColumn<QString>("expectedName");

    QTest::newRow("muted") << 0.8 << true
                            << QStringLiteral("audio-volume-muted");
    QTest::newRow("low") << 0.2 << false
                          << QStringLiteral("audio-volume-low");
    QTest::newRow("medium") << 0.5 << false
                             << QStringLiteral("audio-volume-medium");
    QTest::newRow("high") << 0.8 << false
                           << QStringLiteral("audio-volume-high");
}

void AudioAppletQmlTests::summaryIconTracksDefaultOutput()
{
    QFETCH(double, volume);
    QFETCH(bool, muted);
    QFETCH(QString, expectedName);

    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    Audio::Snapshot snapshot = clientSnapshot();
    snapshot.outputs[0].volume = volume;
    snapshot.outputs[0].muted = muted;
    transport.reply(transport.fetches.constLast(), snapshot);

    AppletHarness harness;
    QString error;
    QVERIFY2(loadApplet(harness, &controller,
                        {QStringLiteral("audio-volume-muted"),
                         QStringLiteral("audio-volume-low"),
                         QStringLiteral("audio-volume-medium"),
                         QStringLiteral("audio-volume-high")},
                        &error),
             qPrintable(error));
    QCOMPARE(harness.applet->property("summaryIconName").toString(),
             expectedName);
    auto *icon = harness.applet->findChild<QQuickItem *>(
        QStringLiteral("audioAppletIcon"));
    QVERIFY(icon != nullptr);
    QVERIFY(Tests::hasResolvedProviderSource(icon, expectedName));
}

QTEST_MAIN(AudioAppletQmlTests)
#include "tst_audio_applet_qml.moc"
