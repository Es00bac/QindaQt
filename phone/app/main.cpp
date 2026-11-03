#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "LinkBridge.h"
#include "VaultBridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // QindaTK controls are QtQuick.Templates with their own visuals; the style only has to exist.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    LinkBridge bridge; // starts the Java service; the engine outlives this UI
    VaultBridge vault; // idle until the Vault page is shown; the Java side owns all vault state and locks
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("link"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("vault"), &vault);
    engine.loadFromModule("QindaLinkApp", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
