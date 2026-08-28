// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QindaQt::Controls::TestSupport::pinDeterministicFonts();

    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    if (!QindaQt::Controls::TestSupport::publishTheme(
            *view.engine(), QStringLiteral("qinda-dark.json"), {}, &error)) {
        QTextStream(stderr) << error << Qt::endl;
        return 2;
    }
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(720, 840);
    view.setSource(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_CONTROLS_TEST_QML_DIR "/ControlsGallery.qml")));
    if (!view.errors().isEmpty()) {
        QTextStream(stderr) << view.errors().constFirst().toString() << Qt::endl;
        return 3;
    }
    view.show();

    QTimer::singleShot(200, &application, [&application]() {
        QTextStream(stdout) << "READY " << application.applicationPid() << Qt::endl;
    });
    QTimer::singleShot(30000, &application, &QCoreApplication::quit);
    return application.exec();
}
