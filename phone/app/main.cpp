#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "LinkBridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // QindaTK controls are QtQuick.Templates with their own visuals; the style only has to exist.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    LinkBridge bridge; // starts the Java service; the engine outlives this UI
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("link"), &bridge);
    engine.loadFromModule("QindaLinkApp", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
