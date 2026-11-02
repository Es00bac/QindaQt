// SPDX-License-Identifier: LGPL-3.0-or-later
// Desktop preview host for the QindaLink phone UI.
//
//   qindalink-preview                         interactive window (needs a display)
//   qindalink-preview --grab DIR [--size 412x915] [--scale 2.625]
//                                             one PNG per scenario and screen into DIR
//   qindalink-preview --smoke                 load every scenario x screen; exit 1 on any QML warning
//
// AGENT-NOTE: the QML is the app's own (phone/app/qml via qml-files.cmake) driven by FakeBridge.
// QindaTK comes from the import path: the installed one, or a newer build via QML_IMPORT_PATH.
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqml.h>

#include "FakeBridge.h"

static int g_warnings = 0;
static QtMessageHandler g_prev = nullptr;
static void countWarnings(QtMsgType t, const QMessageLogContext &c, const QString &m)
{
    if (t == QtWarningMsg || t == QtCriticalMsg || t == QtFatalMsg)
        ++g_warnings;
    g_prev(t, c, m);
}

static void settle(QQuickWindow *w)
{
    // Layout, bindings and the theme's first apply need a few event-loop turns.
    for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QTimer t; t.setSingleShot(true); t.start(60);
    while (t.isActive()) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    w->contentItem(); // keep the window referenced
}

int main(int argc, char *argv[])
{
    QString grabDir, size = QStringLiteral("412x915"), scale;
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&]() { return i + 1 < argc ? QString::fromLocal8Bit(argv[++i]) : QString(); };
        if (a == QLatin1String("--grab")) grabDir = next();
        else if (a == QLatin1String("--size")) size = next();
        else if (a == QLatin1String("--scale")) scale = next();
        else if (a == QLatin1String("--smoke")) smoke = true;
    }
    if (!scale.isEmpty()) qputenv("QT_SCALE_FACTOR", scale.toUtf8());
    if (smoke || !grabDir.isEmpty())
        qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? "offscreen" : qgetenv("QT_QPA_PLATFORM"));

    QGuiApplication app(argc, argv);
    g_prev = qInstallMessageHandler(countWarnings);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    FakeBridge bridge;
    bridge.setScenario(QStringLiteral("ready"));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("link"), &bridge);
    engine.loadFromModule("QindaLinkApp", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!win)
        return 1;
    if (!smoke && grabDir.isEmpty())
        return app.exec();

    const QStringList wh = size.split(QLatin1Char('x'));
    win->resize(wh.value(0).toInt(), wh.value(1).toInt());
    win->show();
    if (!grabDir.isEmpty())
        QDir().mkpath(grabDir);

    const QStringList pages = {"home", "pairing", "permissions", "tools", "touchpad", "keyboard", "macros", "vault"};
    int failures = 0, shots = 0;
    for (const QString &scenario : FakeBridge::scenarios()) {
        bridge.setScenario(scenario);
        for (const QString &page : pages) {
            // Tools pages do not depend on the engine; grab them once.
            if (scenario != QLatin1String("ready") && (page == "touchpad" || page == "keyboard"
                    || page == "macros" || page == "vault" || page == "tools"))
                continue;
            win->setProperty("page", page);
            settle(win);
            auto *loader = win->findChild<QQuickItem *>(QStringLiteral("pageLoader"));
            const QVariant item = loader ? loader->property("item") : QVariant();
            if (!loader || item.value<QObject *>() == nullptr) {
                fprintf(stderr, "FAIL %s/%s: page did not load\n", qPrintable(scenario), qPrintable(page));
                ++failures;
                continue;
            }
            if (!grabDir.isEmpty()) {
                const QString path = QStringLiteral("%1/%2-%3.png").arg(grabDir, scenario, page);
                if (!win->grabWindow().save(path)) { ++failures; fprintf(stderr, "FAIL save %s\n", qPrintable(path)); }
                else ++shots;
            }
        }
    }
    printf("preview: %d screens grabbed, %d failures, %d QML warnings\n", shots, failures, g_warnings);
    return (failures || g_warnings) ? 1 : 0;
}
