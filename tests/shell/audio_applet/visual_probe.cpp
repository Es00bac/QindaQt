// SPDX-License-Identifier: GPL-3.0-or-later

// Headless capture of the production audio applet popup, so a redesign can be
// reviewed as two pictures rather than as a diff. It renders the real compiled
// QML against the real controller over a fake transport; nothing here touches
// a session bus, a running service, or any user state.
//
// AGENT-NOTE: it renders AudioAppletPanel.qml — the popup BODY — directly in
// a plain window, rather than opening the real `T.Popup`. The popup is a
// `popupType: T.Popup.Window`, and its own window sizes itself from a height
// binding that resolves before the Repeater bands have laid out, so a capture
// taken through it cropped the last console strip. Rendering the panel is
// still production QML and gives a deterministic, uncropped frame; the popup's
// own sizing is exercised by the offscreen row in tst_audio_applet_qml.cpp.
//
// argv: <themeJson> <outputPng> [width] [height]  (height 0 = fit the content)

#include "audio_applet_controller.h"
#include "support/fake_audio_transport.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QTest>

#include <cstdio>
#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AudioAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::AudioApplet;
using QindaQt::Tests::clientSnapshot;
using QindaQt::Tests::FakeAudioTransport;

namespace {

const QString kOwner = QStringLiteral(":1.42");

// The shared fixture publishes no console, so the CONSOLE band would be empty
// in every capture. Three strips and two buses with distinct, known readings
// show the band as it actually looks in use.
Audio::Snapshot probeSnapshot()
{
    Audio::Snapshot snapshot = clientSnapshot();
    snapshot.capabilities |= Audio::Capability::Console;
    snapshot.capabilities |= Audio::Capability::SetConsoleGain;
    snapshot.capabilities |= Audio::Capability::SetConsoleRouting;
    snapshot.capabilities |= Audio::Capability::ConsoleMeters;

    const auto strip = [](const char *id, const char *label,
                          Audio::StripKind kind, quint32 index, double gainDb,
                          double peakDb, bool muted) {
        Audio::Strip value;
        value.id = QLatin1String(id);
        value.label = QLatin1String(label);
        value.kind = kind;
        value.index = index;
        value.gainDb = gainDb;
        value.muted = muted;
        value.sourceKnown = kind == Audio::StripKind::HardwareInput;
        value.level = Audio::Level{
            .peakDb = peakDb, .rmsDb = peakDb - 6.0, .known = true};
        value.sends = {
            Audio::MatrixSend{.busIndex = 0, .enabled = true, .gainDb = 0.0},
            Audio::MatrixSend{.busIndex = 1, .enabled = false, .gainDb = -6.0}};
        return value;
    };
    snapshot.console.strips = {
        strip("s1", "Seiren Mini", Audio::StripKind::HardwareInput, 0, 0.0,
              -9.0, false),
        strip("s2", "Desk mic", Audio::StripKind::HardwareInput, 1, -3.5, -22.0,
              false),
        strip("v1", "Game", Audio::StripKind::VirtualInput, 0, 2.0, -2.5, true),
    };

    const auto bus = [](const char *id, const char *label, Audio::BusKind kind,
                        quint32 index, double gainDb, double peakDb) {
        Audio::Bus value;
        value.id = QLatin1String(id);
        value.label = QLatin1String(label);
        value.kind = kind;
        value.index = index;
        value.gainDb = gainDb;
        value.targetKnown = kind == Audio::BusKind::Physical;
        value.level = Audio::Level{
            .peakDb = peakDb, .rmsDb = peakDb - 8.0, .known = true};
        return value;
    };
    snapshot.console.buses = {
        bus("a1", "A1", Audio::BusKind::Physical, 0, 0.0, -11.0),
        bus("b1", "B1", Audio::BusKind::Virtual, 1, -1.5, -30.0),
    };
    return snapshot;
}

bool publishTheme(QQmlEngine &engine, const QString &themeJson,
                  std::unique_ptr<QObject> &registration, QString *error)
{
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                      QUrl(QStringLiteral("inline:token-registration.qml")));
    if (!QTest::qWaitFor([&component] {
            return component.status() != QQmlComponent::Loading;
        })) {
        *error = QStringLiteral("timed out loading QindaQt.Tokens");
        return false;
    }
    if (!component.isReady()) {
        *error = component.errorString();
        return false;
    }
    registration.reset(component.create());
    if (registration == nullptr) {
        *error = QStringLiteral("the QindaQt.Tokens registration failed");
        return false;
    }
    auto *facade =
        engine.singletonInstance<DesignTokens::TokenFacade *>("QindaQt.Tokens",
                                                              "Tokens");
    if (facade == nullptr) {
        *error = QStringLiteral("the Tokens singleton did not resolve");
        return false;
    }
    const auto loaded = Themes::ThemeLoader::fromFile(themeJson);
    if (!loaded.ok) {
        *error = loaded.error;
        return false;
    }
    return facade->publish(loaded.theme, {}, error);
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 3) {
        std::fputs("usage: visual_probe <themeJson> <outputPng>\n", stderr);
        return 2;
    }
    const QString themeJson = QString::fromLocal8Bit(argv[1]);
    const QString output = QString::fromLocal8Bit(argv[2]);

    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    AudioAppletController controller(&client, true, true);
    client.start();
    transport.announceOwner(kOwner);
    if (transport.fetches.isEmpty())
        return 3;
    transport.reply(transport.fetches.constLast(), probeSnapshot());
    if (controller.phaseText() != QStringLiteral("ready")) {
        std::fprintf(stderr, "applet phase is '%s', expected ready\n",
                     qPrintable(controller.phaseText()));
        return 4;
    }

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_AUDIO_APPLET_QML_IMPORT_PATH));
    std::unique_ptr<QObject> registration;
    QString error;
    if (!publishTheme(engine, themeJson, registration, &error)) {
        std::fprintf(stderr, "theme: %s\n", qPrintable(error));
        return 5;
    }

    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.AudioApplet"),
                             QStringLiteral("AudioApplet"));
    if (!component.isReady()) {
        std::fprintf(stderr, "applet: %s\n", qPrintable(component.errorString()));
        return 6;
    }
    std::unique_ptr<QObject> appletObject(component.createWithInitialProperties(
        {{QStringLiteral("controller"), QVariant::fromValue(&controller)}}));
    auto *applet = qobject_cast<QQuickItem *>(appletObject.get());
    if (applet == nullptr) {
        std::fprintf(stderr, "applet: %s\n", qPrintable(component.errorString()));
        return 7;
    }

    // The panel needs the same band state the applet root would own. A plain
    // QtObject with those properties is the smallest honest stand-in; the
    // panel reads them and writes them back exactly as it does in the shell.
    QQmlComponent storeComponent(&engine);
    storeComponent.setData(R"qml(
        import QtQuick
        QtObject {
            property bool outputExpanded: true
            property bool inputExpanded: true
            property bool appsExpanded: true
            property bool consoleExpanded: true
            property int outputSelectedSerial: 0
            property int inputSelectedSerial: 0
            property var desktopControls: null
        }
    )qml",
                           QUrl(QStringLiteral("inline:applet-store.qml")));
    if (!storeComponent.isReady()) {
        std::fprintf(stderr, "store: %s\n",
                     qPrintable(storeComponent.errorString()));
        return 9;
    }
    std::unique_ptr<QObject> store(storeComponent.create());
    if (store == nullptr)
        return 10;

    QQmlComponent panelComponent(&engine);
    panelComponent.loadFromModule(QStringLiteral("QindaQt.Shell.AudioApplet"),
                                  QStringLiteral("AudioAppletPanel"));
    if (!panelComponent.isReady()) {
        std::fprintf(stderr, "panel: %s\n",
                     qPrintable(panelComponent.errorString()));
        return 11;
    }
    std::unique_ptr<QObject> panelObject(
        panelComponent.createWithInitialProperties(
            {{QStringLiteral("controller"), QVariant::fromValue(&controller)},
             {QStringLiteral("store"), QVariant::fromValue(store.get())}}));
    auto *panel = qobject_cast<QQuickItem *>(panelObject.get());
    if (panel == nullptr) {
        std::fprintf(stderr, "panel: %s\n",
                     qPrintable(panelComponent.errorString()));
        return 12;
    }

    // AGENT-GUARD: the backdrop is the popup's own surface token, not a
    // literal. A hardcoded dark colour rendered the light theme as dark text
    // on a dark panel, which says nothing true about either theme.
    QQmlComponent backdropComponent(&engine);
    backdropComponent.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        Rectangle { color: Tokens.bg.raised }
    )qml",
                              QUrl(QStringLiteral("inline:applet-backdrop.qml")));
    if (!backdropComponent.isReady()) {
        std::fprintf(stderr, "backdrop: %s\n",
                     qPrintable(backdropComponent.errorString()));
        return 14;
    }
    std::unique_ptr<QObject> backdropObject(backdropComponent.create());
    auto *backdrop = qobject_cast<QQuickItem *>(backdropObject.get());
    if (backdrop == nullptr)
        return 15;

    const int width = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 420;
    QQuickWindow host;
    backdrop->setParentItem(host.contentItem());
    panel->setParentItem(backdrop);
    panel->setX(12);
    panel->setY(12);
    panel->setWidth(width - 24);
    host.setWidth(width);
    host.setHeight(argc > 4 && QString::fromLocal8Bit(argv[4]).toInt() > 0
                       ? QString::fromLocal8Bit(argv[4]).toInt()
                       : int(panel->implicitHeight()) + 24);
    backdrop->setWidth(host.width());
    backdrop->setHeight(host.height());
    host.show();
    if (!QTest::qWaitFor([&host] { return host.isExposed(); }))
        return 13;
    // The console meters hold a falling peak on a 100 ms timer; one settled
    // frame keeps a capture from recording mid-decay.
    QTest::qWait(400);
    std::fprintf(stdout, "panel implicitHeight=%.1f height=%.1f window=%dx%d\n",
                 panel->implicitHeight(), panel->height(), host.width(),
                 host.height());

    const QImage image = host.grabWindow();
    if (image.isNull())
        return 13;
    if (!image.save(output))
        return 14;
    std::fprintf(stdout, "captured %dx%d to %s\n", image.width(),
                 image.height(), qPrintable(output));
    return 0;
}
