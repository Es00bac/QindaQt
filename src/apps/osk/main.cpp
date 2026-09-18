// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_method_client.h"
#include "input_panel_integration.h"
#include "osk_appearance.h"
#include "osk_evidence.h"
#include "osk_keyboard_model.h"
#include "osk_layout_catalog.h"
#include "osk_layout_source.h"
#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QUrl>
#include <cstdio>

namespace {

using namespace QindaQt::Apps::Osk;

void addImportPaths(QQmlEngine &engine)
{
    const QString current = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString build = QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH)).canonicalFilePath();
    if (!build.isEmpty() && current == build) {
        engine.addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    }
    engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
}

// Content purposes that want the digits page first (text-input v3 values,
// which the input-method v1 context relays unchanged).
bool numericPurpose(quint32 purpose)
{
    return purpose == 2 /* digits */ || purpose == 3 /* number */ || purpose == 4 /* phone */;
}

struct Panel {
    QQuickView &view;
    OskKeyboardModel &keyboard;
    InputMethodV1 &inputMethod;
    OskEvidence &evidence;

    void sync()
    {
        const bool activated = inputMethod.activated();
        if (activated) {
            if (auto *context = inputMethod.context()) {
                QObject::connect(context, &InputMethodContextV1::contentTypeChanged, &view, [this, context] {
                    keyboard.showSymbols(numericPurpose(context->contentPurpose()));
                });
            }
            view.show();
        } else {
            keyboard.resetTransientState();
            view.hide();
        }
        evidence.setActivated(activated);
        evidence.setVisible(view.isVisible());
        evidence.write(keyboard);
    }
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("QindaQt"));
    application.setOrganizationDomain(QStringLiteral("qindaqt.org"));
    application.setApplicationName(QStringLiteral("qindaqt-osk"));
    application.setDesktopFileName(QStringLiteral("org.qindaqt.OnScreenKeyboard"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QindaQt on-screen keyboard (the compositor's input method)"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("theme-directory"), QStringLiteral("Additional local theme directory"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("layout"), QStringLiteral("Use this XKB layout name instead of the compositor's"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("evidence"), QStringLiteral("Write the keyboard's state to this JSON file (nested harness)"), QStringLiteral("file")});
    parser.process(application);

    if (application.platformName() != QLatin1String("wayland")) {
        std::fprintf(stderr, "qindaqt-osk: needs a Wayland session, got platform '%s'\n", qPrintable(application.platformName()));
        return 2;
    }

    InputMethodV1 inputMethod;
    InputPanelV1 inputPanel;
    inputMethod.bindNow();
    inputPanel.bindNow();
    if (!inputMethod.isActive() || !inputPanel.isActive()) {
        std::fprintf(stderr, "qindaqt-osk: the compositor offers no zwp_input_method_v1/zwp_input_panel_v1 on this connection;"
                             " it must launch qindaqt-osk itself as its input method ([Wayland] InputMethod=)\n");
        return 4;
    }

    const QDBusConnection bus = QDBusConnection::sessionBus();
    QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport(bus);
    QindaQt::Services::SettingsClient::SettingsClient settingsClient(
        settingsTransport, QStringList{QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme")});
    QString settingsError;
    if (!settingsClient.start(&settingsError)) {
        qWarning("qindaqt-osk: Settings1 unavailable: %s", qPrintable(settingsError));
    }
    QindaQt::AppAppearance::ApplicationAppearanceController appearance(
        settingsClient, QindaQt::AppAppearance::standardThemeDirectories(parser.value(QStringLiteral("theme-directory"))),
        QStringLiteral("qinda-dark"));

    QQuickView view;
    addImportPaths(*view.engine());
    QString error;
    auto *facade = ensureOskTokenFacade(*view.engine(), &error);
    if (facade == nullptr || !applyOskAppearance(appearance, *facade, application, &error)) {
        std::fprintf(stderr, "qindaqt-osk: appearance bootstrap failed: %s\n", qPrintable(error));
        return 3;
    }
    QObject::connect(&appearance, &QindaQt::AppAppearance::ApplicationAppearanceController::appearanceChanged, &view,
                     [&appearance, facade, &application] {
                         QString applyError;
                         if (!applyOskAppearance(appearance, *facade, application, &applyError)) {
                             qWarning("qindaqt-osk: appearance update failed: %s", qPrintable(applyError));
                         }
                     });

    OskKeyboardModel keyboard;
    keyboard.setEmitter(&inputMethod);
    OskLayoutCatalog catalog;
    OskLayoutSource layoutSource(bus);
    const QString forcedLayout = parser.value(QStringLiteral("layout"));
    const auto applyLayout = [&] {
        const QString name = forcedLayout.isEmpty() ? layoutSource.currentLayout() : forcedLayout;
        QString layoutError;
        const OskLayoutDocument document = catalog.documentFor(name, &layoutError);
        if (!document.isValid()) {
            std::fprintf(stderr, "qindaqt-osk: layout '%s' unusable: %s\n", qPrintable(name), qPrintable(layoutError));
            return;
        }
        if (document.name != keyboard.layoutName()) {
            keyboard.setDocument(document);
        }
    };
    QObject::connect(&layoutSource, &OskLayoutSource::currentLayoutChanged, &keyboard, applyLayout);
    QObject::connect(&keyboard, &OskKeyboardModel::nextLayoutRequested, &layoutSource, [&] {
        if (layoutSource.available() && layoutSource.layouts().size() > 1) {
            layoutSource.switchToNext();
            return;
        }
        // No compositor layout list: cycle the keyboard's own documents.
        const QStringList names = catalog.available();
        const qsizetype next = (names.indexOf(keyboard.layoutName()) + 1) % std::max<qsizetype>(1, names.size());
        keyboard.setDocument(catalog.documentFor(names.value(next)));
    });
    applyLayout();
    if (forcedLayout.isEmpty()) {
        layoutSource.refresh();
    }

    OskEvidence evidence(parser.isSet(QStringLiteral("evidence")) ? parser.value(QStringLiteral("evidence"))
                                                                    : qEnvironmentVariable("QINDAQT_OSK_EVIDENCE_FILE"));
    InputPanelShellIntegration integration(inputPanel);
    view.setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    view.setResizeMode(QQuickView::SizeViewToRootObject);
    view.rootContext()->setContextProperty(QStringLiteral("keyboard"), &keyboard);
    view.setSource(QUrl(QStringLiteral("qrc:/qt/qml/QindaQt/OskApp/qml/OnScreenKeyboard.qml")));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError &qmlError : view.errors()) {
            std::fprintf(stderr, "qindaqt-osk: %s\n", qPrintable(qmlError.toString()));
        }
        return 5;
    }
    if (!integration.attach(&view)) {
        std::fprintf(stderr, "qindaqt-osk: could not give the keyboard window the input-panel role\n");
        return 4;
    }

    Panel panel{view, keyboard, inputMethod, evidence};
    QObject::connect(&inputMethod, &InputMethodV1::activatedChanged, &view, [&panel] { panel.sync(); });
    QObject::connect(&keyboard, &OskKeyboardModel::hideRequested, &view, [&] {
        view.hide();
        evidence.setVisible(false);
        evidence.write(keyboard);
    });
    QObject::connect(&keyboard, &OskKeyboardModel::rowsChanged, &view, [&] { evidence.write(keyboard); });
    QObject::connect(&keyboard, &OskKeyboardModel::keyPressed, &view, [&](const QString &kind, const QString &text) {
        evidence.recordPress(kind, text);
        evidence.write(keyboard);
    });
    panel.sync();
    return application.exec();
}
