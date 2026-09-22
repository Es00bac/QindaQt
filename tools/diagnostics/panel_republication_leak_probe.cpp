// Does destroying and recreating a panel-like QQuickWindow set leak threads or
// GL contexts? This is the Qt-vs-QindaQt discriminator OPEN-DEFECTS.md item 5
// asks for, with the output count held FIXED so the confound is removed.
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QTimer>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <memory>

static int threadCount()
{
    return QDir(QStringLiteral("/proc/self/task"))
        .entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
}

static long rssKb()
{
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly)) return -1;
    for (const QByteArray &line : f.readAll().split('\n'))
        if (line.startsWith("VmRSS:")) return line.split(':').at(1).trimmed().split(' ').at(0).toLong();
    return -1;
}

static void spin(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const int generations = qEnvironmentVariableIntValue("LEAK_GENERATIONS") ?: 8;
    const int panels = qEnvironmentVariableIntValue("LEAK_PANELS") ?: 3;

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        Window { width: 400; height: 40; color: "#202020"
                 Rectangle { anchors.fill: parent; color: "#3080c0" } }
    )", QUrl());
    if (component.isError()) { qWarning() << component.errorString(); return 2; }

    qInfo("generation threads rssKb");
    qInfo("baseline   %7d %7ld", threadCount(), rssKb());

    for (int generation = 1; generation <= generations; ++generation) {
        std::vector<std::unique_ptr<QQuickWindow>> windows;
        for (int i = 0; i < panels; ++i) {
            auto *w = qobject_cast<QQuickWindow *>(component.create());
            if (!w) { qWarning("create failed"); return 3; }
            w->setFlag(Qt::FramelessWindowHint);
            w->setVisible(true);
            windows.emplace_back(w);
        }
        spin(220);                       // let them map and render
        windows.clear();                 // destroy the whole set, as republish does
        spin(220);                       // let Qt/Mesa release
        qInfo("gen %-6d %7d %7ld", generation, threadCount(), rssKb());
    }
    return 0;
}
