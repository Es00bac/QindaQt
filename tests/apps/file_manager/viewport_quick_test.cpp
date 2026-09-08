// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtQuickTest/quicktest.h>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QIcon>
#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/themes/theme_loader.h>

// Test-only token composition: real compiled Controls and Tokens, fixture
// theme and icons, no application launch or host settings/session transport.
class ViewportSetup : public QObject {
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        const QString sourceRoot = QString::fromUtf8(QINDAQT_SOURCE_ROOT);
        engine->addImportPath(QString::fromUtf8(QINDAQT_QML_IMPORT_PATH));
        QQmlComponent registration(engine);
        registration.setData("import QtQuick\nimport QindaQt.Tokens 1.0\n"
                             "QtObject { property bool ready: Tokens.ready }",
                             QUrl("inline:viewport-tokens.qml"));
        while (registration.isLoading()) QCoreApplication::processEvents();
        std::unique_ptr<QObject> object(registration.create());
        if (!object) qFatal("Could not load viewport test Tokens");
        auto *tokens = engine->singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
            "QindaQt.Tokens", "Tokens");
        const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
            sourceRoot + "/data/themes/" + qEnvironmentVariable("QINDAQT_VIEWPORT_TEST_THEME", "qinda-light") + ".json");
        QString error;
        QindaQt::DesignTokens::AccessibilityInputs access;
        access.highContrast = theme.theme.variant == "high-contrast";
        if (!theme.ok || !tokens || !tokens->publish(theme.theme, access, &error))
            qFatal("Could not publish viewport test theme");
        QIcon::setThemeSearchPaths({sourceRoot + "/data/icons"});
        QIcon::setThemeName(theme.theme.iconTheme);
    }
};
QUICK_TEST_MAIN_WITH_SETUP(file_manager_viewport, ViewportSetup)
#include "viewport_quick_test.moc"
