// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/prctl.h>
#include <sys/resource.h>
#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QQuickWindow>
int main(int argc,char **argv) {
    struct rlimit core{0,0};
    if(setrlimit(RLIMIT_CORE,&core)!=0 || prctl(PR_SET_DUMPABLE,0)!=0) return 2;
    QGuiApplication app(argc,argv);QQuickWindow window;window.setColor(Qt::white);window.resize(160,100);window.show();window.requestActivate();
    QTimer::singleShot(600,&app,[&app] {
        QString text=app.clipboard()->text();
        const bool expected=qEnvironmentVariable("QINDAQT_JOURNEY_CLIPBOARD_EXPECT")=="secret";
        const bool okay=expected?text==QStringLiteral("fixture-secret-only"):text.isEmpty();
        text.fill(QChar(0));text.clear();app.exit(okay?0:5);
    });return app.exec();
}
