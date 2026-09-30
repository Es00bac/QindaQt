// SPDX-License-Identifier: GPL-3.0-or-later
#include "idle_fixture_server.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QTextStream>
#include <QSocketNotifier>
#include <sys/stat.h>
#include <unistd.h>
class NativeState : public QObject {
    Q_OBJECT
public:
    bool locked=false,protectedPresentation=false;
};
class NativeAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.qindaqt.KWin.NativeLock1")
    Q_PROPERTY(bool Locked READ locked)
    Q_PROPERTY(bool Protected READ protectedPresentation)
public:
    explicit NativeAdaptor(NativeState &state):QDBusAbstractAdaptor(&state),state_(state){}
    bool locked() const{return state_.locked;}
    bool protectedPresentation() const{return state_.protectedPresentation;}
    void publish(bool lockedValue,bool protectedValue){
        state_.locked=lockedValue;state_.protectedPresentation=protectedValue;
        emit lockedChanged(lockedValue);emit protectedChanged(protectedValue);
    }
Q_SIGNALS:
    void lockedChanged(bool);
    void protectedChanged(bool);
private:
    NativeState &state_;
};
int main(int argc,char **argv){
    umask(0077);QCoreApplication app(argc,argv);
    if(argc!=2) return 2;
    auto bus=QDBusConnection::sessionBus();
    if(!bus.registerService(QString(QindaQt::CompositorNames::service))) return 3;
    Server server;
    if(wl_display_add_socket(server.display,argv[1])!=0) return 3;
    NativeState state;NativeAdaptor adaptor(state);
    if(!bus.registerObject(QStringLiteral("/org/qindaqt/KWin/NativeLock"),&state,QDBusConnection::ExportAdaptors)) return 3;
    QSocketNotifier input(STDIN_FILENO,QSocketNotifier::Read);
    QObject::connect(&input,&QSocketNotifier::activated,&app,[&]{
        char buffer[64];const auto count=read(STDIN_FILENO,buffer,sizeof(buffer));
        if(count<=0){app.quit();return;}
        const auto command=QByteArray(buffer,static_cast<qsizetype>(count)).trimmed();
        if(command=="lock") adaptor.publish(true,true);
        else if(command=="locking") adaptor.publish(true,false);
        else if(command=="unlock") adaptor.publish(false,false);
        else if(command=="idle" && server.notification) server.idled();
        else if(command=="resume" && server.notification) server.resumed();
        else if(command=="timeout") {QTextStream(stdout)<<"timeout:"<<server.timeout<<"\n"<<Qt::flush;return;}
        else if(command=="release") bus.unregisterService(QString(QindaQt::CompositorNames::service));
        else if(command=="drop") wl_display_destroy_clients(server.display);
        else if(command=="quit"){app.quit();return;}
        QTextStream(stdout)<<"done\n"<<Qt::flush;
    });
    QTextStream(stdout)<<"ready\n"<<Qt::flush;return app.exec();
}
#include "policy_compositor.moc"
