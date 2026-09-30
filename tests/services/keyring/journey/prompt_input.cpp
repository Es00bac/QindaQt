// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
// AGENT-CONTRACT: This test-only input driver links the production prompt main,
// controller and QML. It injects visible control events; only that controller
// can approve/write the normal protocol. Fixed synthetic bytes never enter env,
// argv, audit output or the host clipboard. It is not installed.
void input() {
    for (auto *base : QGuiApplication::allWindows()) {
        auto *window=qobject_cast<QQuickWindow *>(base);
        if (!window || window->objectName()!=QStringLiteral("keyringPromptWindow")
            || !window->isExposed()) continue;
        auto *native=qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        ucred peer{};socklen_t size=sizeof(peer);
        if (!native || getsockopt(wl_display_get_fd(native->display()),SOL_SOCKET,SO_PEERCRED,&peer,&size)!=0
            || peer.pid!=qEnvironmentVariableIntValue("QINDAQT_JOURNEY_COMPOSITOR_PID")) {
            QCoreApplication::exit(3);return;
        }
        QFile phase(qEnvironmentVariable("QINDAQT_JOURNEY_PHASE"));
        bool after=phase.open(QIODevice::ReadOnly) && phase.readAll().trimmed()=="1";
        const QString current=after?QStringLiteral("unit-test-only-other-password"):QStringLiteral("unit-test-only-password");
        const bool changing=QCoreApplication::arguments().contains(QStringLiteral("change-password"));
        auto type=[window](const char *name,const QString &text) {
            auto *field=window->findChild<QQuickItem *>(QString::fromLatin1(name));
            if (!field || !field->isVisible()) return;
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,field->mapToScene(QPointF(field->width()/2,field->height()/2)).toPoint());
            for (const QChar c:text) QTest::keyClick(window,c.toLatin1());
        };
        type("keyringOldPasswordField",current);
        const QString next=changing?QStringLiteral("unit-test-only-other-password"):current;
        type("keyringPasswordField",next);type("keyringConfirmationField",next);
        auto *button=window->findChild<QQuickItem *>(QStringLiteral("keyringApproveButton"));
        if (!button || !button->isEnabled()) {QCoreApplication::exit(4);return;}
        QFile audit(qEnvironmentVariable("QINDAQT_JOURNEY_PROMPT_AUDIT"));
        if(audit.open(QIODevice::WriteOnly|QIODevice::Append)) {
            const auto args=QCoreApplication::arguments();const auto action=args.indexOf(QStringLiteral("--action"));
            audit.write("ordinary-wayland exposed exact-peer visible-approval "+args.value(action+1).toUtf8()+"\n");
        }
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        return;
    }
    QTimer::singleShot(100,qApp,input);
}
void setup(){QTimer::singleShot(100,QCoreApplication::instance(),input);}
Q_COREAPP_STARTUP_FUNCTION(setup)
}
