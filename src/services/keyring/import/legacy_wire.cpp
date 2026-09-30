// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_wire_p.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include <sys/syscall.h>
#include <unistd.h>
#include <poll.h>
#include <climits>
#include <algorithm>
namespace qindaqt::keyring::importer {
namespace {
constexpr auto Daemon="org.freedesktop.DBus";
bool owner(const QString &name) {static const QRegularExpression pattern("^:[0-9]+\\.[0-9]+$");return name.size()<=128 && pattern.match(name).hasMatch();}
bool sender(DBusMessage *message,const QString &expected) {
    const char *actual=dbus_message_get_sender(message);return actual && expected==QString::fromUtf8(actual);
}
bool mutation(DBusMessage *message) {
    const char *interface=dbus_message_get_interface(message);
    const char *member=dbus_message_get_member(message);
    const QString api=QString::fromUtf8(interface?interface:"");
    const QString event=QString::fromUtf8(member?member:"");
    return ((api=="org.freedesktop.DBus.Properties" && event=="PropertiesChanged")
                || (api=="org.freedesktop.Secret.Service" && (event=="CollectionCreated" || event=="CollectionDeleted" || event=="CollectionChanged"))
                || (api=="org.freedesktop.Secret.Collection" && (event=="ItemCreated" || event=="ItemDeleted" || event=="ItemChanged"))
                || (api=="org.kde.KWallet" && (event=="folderUpdated" || event=="folderListUpdated" || event=="walletListDirty" || event=="walletCreated" || event=="walletDeleted")));
}
}
Wire::Wire(const QString &address,LegacySourceBinding binding,const std::function<bool()> &admitted)
    :binding_(std::move(binding)),admitted_(admitted) {
    if(address.isEmpty() || address.size()>4096 || address.contains(QChar(0)) || !owner(binding_.uniqueOwner) || !owner(binding_.sessionOwner) || !binding_.pid || binding_.pid>INT_MAX
        || (binding_.kind!="secret-service" && binding_.kind!="kwallet")
        || (binding_.kind=="secret-service" && binding_.service!="org.freedesktop.secrets")
        || (binding_.kind=="kwallet" && binding_.service!="org.kde.kwalletd6" && binding_.service!="org.kde.kwalletd5"))
        throw Failure{CollectionImportError::InvalidInput};
    budget_.start();DBusError error;dbus_error_init(&error);
    connection_=dbus_connection_open_private(address.toUtf8().constData(),&error);
    dbus_error_free(&error);
    if(!connection_) throw Failure{CollectionImportError::Unavailable};
    dbus_connection_set_exit_on_disconnect(connection_,false);
    dbus_connection_set_max_message_size(connection_,8*1024*1024);
    dbus_connection_set_max_received_size(connection_,16*1024*1024);
    try {
        dbus_error_init(&error);const bool registered=dbus_bus_register(connection_,&error);dbus_error_free(&error);
        if(!registered) throw Failure{CollectionImportError::Unavailable};
        pidfd_=static_cast<int>(syscall(SYS_pidfd_open,static_cast<pid_t>(binding_.pid),0));
        if(pidfd_<0 || daemonNumber("GetConnectionUnixUser",binding_.uniqueOwner)!=geteuid()
            || daemonNumber("GetConnectionUnixUser",binding_.sessionOwner)!=geteuid()) throw Failure{CollectionImportError::OwnerLost};
        for(const auto &match:{QString("type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged'"),
            QString("type='signal',sender='")+binding_.uniqueOwner+"'"}) {
            dbus_error_init(&error);dbus_bus_add_match(connection_,match.toUtf8().constData(),&error);
            const bool failed=dbus_error_is_set(&error);dbus_error_free(&error);
            if(failed) throw Failure{CollectionImportError::Unavailable};
        }
        if(!live()) throw Failure{CollectionImportError::OwnerLost};
    } catch(...) {
        if(pidfd_>=0) close(pidfd_);
        pidfd_=-1;
        dbus_connection_close(connection_);dbus_connection_unref(connection_);connection_=nullptr;throw;
    }
}
Wire::~Wire() {
    signals_.clear();if(pidfd_>=0) close(pidfd_);
    if(connection_) {dbus_connection_close(connection_);dbus_connection_unref(connection_);}
}
Message Wire::raw(const QString &destination,const QString &path,const QString &interface,const QString &method,const Append &append,int timeout) {
    const auto remaining=600000-budget_.elapsed();
    if(retired_ || remaining<=0 || !dbus_connection_get_is_connected(connection_)) throw Failure{CollectionImportError::OwnerLost};
    Message request(dbus_message_new_method_call(destination.toUtf8().constData(),path.toUtf8().constData(),
        interface.toUtf8().constData(),method.toUtf8().constData()));
    if(!request) throw Failure{CollectionImportError::Unavailable};
    dbus_message_set_auto_start(request.get(),false);
    if(append) {auto iter=begin(request.get());dbus_message_iter_init_append(request.get(),&iter);append(iter);}
    DBusError error;dbus_error_init(&error);
    Message reply(dbus_connection_send_with_reply_and_block(connection_,request.get(),static_cast<int>(std::min<qint64>(timeout,remaining)),&error));
    dbus_error_free(&error);
    if(!reply) throw Failure{CollectionImportError::Unavailable};
    return reply;
}
QString Wire::daemonText(const QString &method,const QString &argument) {
    auto reply=raw(Daemon,"/org/freedesktop/DBus",Daemon,method,[&](auto &i){appendText(i,argument);},1500);
    if(!sender(reply.get(),Daemon) || dbus_message_get_type(reply.get())!=DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(reply.get(),"s")) throw Failure{CollectionImportError::OwnerLost};
    auto iter=begin(reply.get());return text(iter);
}
quint64 Wire::daemonNumber(const QString &method,const QString &argument) {
    auto reply=raw(Daemon,"/org/freedesktop/DBus",Daemon,method,[&](auto &i){appendText(i,argument);},1500);
    if(!sender(reply.get(),Daemon) || dbus_message_get_type(reply.get())!=DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(reply.get(),"u")) throw Failure{CollectionImportError::OwnerLost};
    auto iter=begin(reply.get());dbus_uint32_t value=0;dbus_message_iter_get_basic(&iter,&value);return value;
}
void Wire::drain() {
    int count=0;
    while(Message message{dbus_connection_pop_message(connection_)}) {
        if(++count>1024) {retired_=true;return;}
        if(dbus_message_is_signal(message.get(),Daemon,"NameOwnerChanged") && sender(message.get(),Daemon)
            && dbus_message_has_signature(message.get(),"sss")) {
            auto iter=begin(message.get());const auto name=text(iter);const auto old=text(iter);const auto current=text(iter);
            if(((name==binding_.service || name==binding_.uniqueOwner) && old==binding_.uniqueOwner && current!=old)
                || (name==binding_.sessionOwner && old==binding_.sessionOwner && current!=old)) retired_=true;
        } else if(dbus_message_get_type(message.get())==DBUS_MESSAGE_TYPE_SIGNAL && sender(message.get(),binding_.uniqueOwner)) {
            // AGENT-GUARD: captured bytes retire on observed data mutation;
            // legacy APIs still require operator quiescence (ADR-0312).
            if(frozen_ && mutation(message.get())) {retired_=true;return;}
            if(signals_.size()>=32) {retired_=true;return;}signals_.push_back(std::move(message));
        }
    }
}
void Wire::freeze() {
    // Earlier notifications include our owned unlock/handle lifecycle and have
    // already been incorporated into both exact passes. Fence later data changes,
    // not historical Locked->Unlocked notifications from the completed capture.
    signals_.clear();frozen_=true;
}
bool Wire::live() {
    try {
        drain();pollfd process{pidfd_,POLLIN,0};
        if(retired_ || !admitted_ || !admitted_() || poll(&process,1,0)!=0) {retired_=true;return false;}
        const bool accepted=daemonText("GetNameOwner",binding_.service)==binding_.uniqueOwner
            && daemonText("GetNameOwner",binding_.sessionOwner)==binding_.sessionOwner
            && daemonNumber("GetConnectionUnixProcessID",binding_.uniqueOwner)==binding_.pid;
        drain();if(!accepted || retired_) {retired_=true;return false;}return true;
    } catch(...) {retired_=true;return false;}
}
Message Wire::call(const QString &path,const QString &interface,const QString &method,const Append &append,const char *signature) {
    QCoreApplication::processEvents();
    if(!live()) throw Failure{CollectionImportError::OwnerLost};
    auto reply=raw(binding_.uniqueOwner,path,interface,method,append,1500);
    if(!sender(reply.get(),binding_.uniqueOwner)) throw Failure{CollectionImportError::OwnerLost};
    if(dbus_message_get_type(reply.get())!=DBUS_MESSAGE_TYPE_METHOD_RETURN) throw Failure{CollectionImportError::Unavailable};
    if(!dbus_message_has_signature(reply.get(),signature)) throw Failure{CollectionImportError::InvalidInput};
    QCoreApplication::processEvents();
    if(!live()) throw Failure{CollectionImportError::OwnerLost};
    return reply;
}
Message Wire::waitSignal(const QString &path,const QString &interface,const QString &member,const char *signature,
        const std::function<bool(DBusMessage *)> &accept,int timeout) {
    QElapsedTimer wait;wait.start();
    while(wait.elapsed()<timeout) {
        QCoreApplication::processEvents();if(!live()) throw Failure{CollectionImportError::OwnerLost};
        for(auto i=signals_.begin();i!=signals_.end();++i) {
            auto *message=i->get();
            if(path==QString::fromUtf8(dbus_message_get_path(message))
                && dbus_message_is_signal(message,interface.toUtf8().constData(),member.toUtf8().constData())) {
                if(!dbus_message_has_signature(message,signature)) throw Failure{CollectionImportError::InvalidInput};
                if(accept(message)) {auto result=std::move(*i);signals_.erase(i);return result;}
            }
        }
        dbus_connection_read_write(connection_,50);
    }
    throw Failure{CollectionImportError::Cancelled};
}
}
