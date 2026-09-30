// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/secret_portal/secret_portal_adaptor.h>
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <QDBusPendingCallWatcher>
#include <QProcess>
#include <QTimer>
#include <QTest>
#include <QFile>
#include <sys/socket.h>
#include <unistd.h>
using namespace QindaQt::Services::SecretPortal;
class Bus final {
public:
    Bus() {daemon.start(DBUS_DAEMON_EXECUTABLE,{"--session","--nofork","--print-address=1"});if(daemon.waitForStarted() && daemon.waitForReadyRead()) address=QString::fromUtf8(daemon.readLine()).trimmed();}
    ~Bus() {for(const auto &name:names) QDBusConnection::disconnectFromBus(name);daemon.terminate();daemon.waitForFinished(5000);}
    QDBusConnection connection() {const auto name="secret-test-"+QString::number(getpid())+"-"+QString::number(names.size());names.append(name);return QDBusConnection::connectToBus(address,name);}
    QProcess daemon;QString address;QStringList names;
};
class Pair final {
public:
    Pair() {if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,fd)!=0) fd[0]=fd[1]=-1;}
    ~Pair() {for(int value:fd) if(value>=0) close(value);}
    void closeRead() {close(fd[0]);fd[0]=-1;}
    QByteArray read() {char buffer[64];const auto n=recv(fd[0],buffer,sizeof(buffer),MSG_DONTWAIT);return n>0?QByteArray(buffer,n):QByteArray{};}
    int fd[2]{-1,-1};
};
inline QString handle(int id) {return "/org/freedesktop/portal/desktop/request/1_2/r"+QString::number(id);}
inline QDBusMessage retrieve(const QString &app,int fd,int id=1,QVariantMap options={}) {
    auto call=QDBusMessage::createMethodCall(BackendName,"/org/freedesktop/portal/desktop",SecretInterface,"RetrieveSecret");
    call.setArguments({QVariant::fromValue(QDBusObjectPath(handle(id))),app,QVariant::fromValue(QDBusUnixFileDescriptor(fd)),options});return call;
}
class MockBroker final:public SecretBroker {
public:
    bool ready=true,hold=false;std::size_t size=SecretSize;int acquired=0,cancelled=0;SecretPages last;QMap<quint64,QString> requests;
    void retrieve(quint64 token,const QString &app) override {
        ++acquired;requests.insert(token,app);
        if(!hold) QTimer::singleShot(0,this,[this,token] {release(token);});
    }
    void release(quint64 token) {
        if(!requests.contains(token)) return;
        requests.remove(token);last=std::make_shared<qindaqt::keyring::SecureBuffer>(size);std::fill(last->bytes().begin(),last->bytes().end(),0x5a);
        Q_EMIT completed(token,last,BrokerError::None);
    }
    void cancel(quint64 token) override {if(requests.remove(token)) ++cancelled;}
    bool admitted() const override {return ready;}
    void lose() {ready=false;Q_EMIT authorityLost();}
};
