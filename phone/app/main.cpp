#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "LinkBridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    LinkBridge bridge; // starts the Java service; the engine outlives this UI
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("link"), &bridge);
    engine.loadFromModule("QindaLinkApp", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
