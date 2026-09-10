// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <QIcon>
#include <QStyleHints>

#include "preview/theme_icon_provider.h"

// Test-only presentation setup: stock Controls (Fusion via the row's
// QT_QUICK_CONTROLS_STYLE), fixture icons, and the theme-icons image provider
// the production delegates resolve icons through. No tokens, no application
// launch, no host settings/session transport.
class ViewportSetup : public QObject {
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        using namespace QindaQt::Apps::FileManager;
        const QString sourceRoot = QString::fromUtf8(QINDAQT_SOURCE_ROOT);
        QIcon::setThemeSearchPaths({sourceRoot + "/data/icons"});
        QIcon::setThemeName(QStringLiteral("QindaQt"));
        if (qEnvironmentVariable("QINDAQT_VIEWPORT_TEST_THEME").contains("dark"))
            QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        engine->addImageProvider(QStringLiteral("theme-icons"), new ThemeIconProvider());
    }
};
QUICK_TEST_MAIN_WITH_SETUP(file_manager_viewport, ViewportSetup)
#include "viewport_quick_test.moc"
