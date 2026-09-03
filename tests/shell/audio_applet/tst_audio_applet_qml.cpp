// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_applet_controller.h"

#include "support/fake_audio_transport.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AudioAppletPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::AudioApplet;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root->objectName() == name) {
        matches.append(root);
    }
    for (QQuickItem *child : root->childItems()) {
        matches.append(visualItemsNamed(child, name));
    }
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

} // namespace

class AudioAppletQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compiledAppletSupportsKeyboardAndAccessibility();
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

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_AUDIO_APPLET_QML_IMPORT_PATH));

    // AGENT-NOTE: QindaQt.Controls resolves QST-1 roles from the read-only
    // Tokens singleton; without a published theme the state cards render
    // undefined tokens. Publication is the same seam production composition
    // uses, exercised here through the generated plugin path.
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    QTRY_VERIFY2(registration.status() != QQmlComponent::Loading,
                 "timed out loading the QindaQt.Tokens module");
    QVERIFY2(registration.isReady(), qPrintable(registration.errorString()));
    std::unique_ptr<QObject> registrationObject(registration.create());
    QVERIFY(registrationObject != nullptr);
    auto *facade = engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    QVERIFY(facade != nullptr);
    const auto loaded = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QString themeError;
    QVERIFY2(facade->publish(loaded.theme, {}, &themeError),
             qPrintable(themeError));

    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.AudioApplet"),
                             QStringLiteral("AudioApplet"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> owned(component.createWithInitialProperties(
        {{QStringLiteral("controller"), QVariant::fromValue(&controller)}}));
    QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(owned.get());
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

    // One slider per device with known volume (output and input) plus one
    // stream slider; every control carries a complete accessible identity.
    const auto deviceSliders =
        visualItemsNamed(window.contentItem(), QStringLiteral("audioDeviceVolume"));
    QCOMPARE(deviceSliders.size(), 2);
    const auto streamSliders =
        visualItemsNamed(window.contentItem(), QStringLiteral("audioStreamVolume"));
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
    QCOMPARE(transport.operations.constFirst().request.volume, 0.55);
    QCOMPARE(countPendingRows(controller), 1);

    transport.finish(transport.operations.constFirst(),
                     successfulResult(transport.operations.constFirst(), 2));
    QTRY_COMPARE(countPendingRows(controller), 0);

    // The mute switch exposes an accessible role and description as well.
    const auto muteSwitches =
        visualItemsNamed(window.contentItem(), QStringLiteral("audioDeviceMute"));
    QCOMPARE(muteSwitches.size(), 2);
    QAccessibleInterface *muteInterface =
        QAccessible::queryAccessibleInterface(muteSwitches.constFirst());
    QVERIFY(muteInterface != nullptr);
    QVERIFY(!muteInterface->text(QAccessible::Description).isEmpty());
}

QTEST_MAIN(AudioAppletQmlTests)
#include "tst_audio_applet_qml.moc"
