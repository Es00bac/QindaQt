// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/legacy_reader.h>
#include <dbus/dbus.h>
#include <QElapsedTimer>
#include <memory>
#include <deque>
namespace qindaqt::keyring::importer {
struct Failure {CollectionImportError error;};
struct MessageDelete {void operator()(DBusMessage *m) const {if(m) dbus_message_unref(m);}};
using Message=std::unique_ptr<DBusMessage,MessageDelete>;
using Append=std::function<void(DBusMessageIter &)>;
void appendText(DBusMessageIter &,const QString &,int type=DBUS_TYPE_STRING);
void appendInt(DBusMessageIter &,int);
void appendPaths(DBusMessageIter &,const QStringList &);
QString text(DBusMessageIter &,int type=DBUS_TYPE_STRING,qsizetype limit=1024);
std::uint64_t unsigned64(DBusMessageIter &);
int integer(DBusMessageIter &);
bool boolean(DBusMessageIter &);
enum class DuplicateText {Reject,Collapse};
QStringList texts(DBusMessageIter &,int type=DBUS_TYPE_STRING,qsizetype count=1024,DuplicateText duplicates=DuplicateText::Reject);
SecureBuffer bytes(DBusMessageIter &,std::size_t maximum=1024*1024);
Attributes stringMap(DBusMessageIter &);
DBusMessageIter begin(DBusMessage *);
DBusMessageIter child(DBusMessageIter &,int expected);
void end(DBusMessageIter &);
class Wire final {
public:
    Wire(const QString &address,LegacySourceBinding,const std::function<bool()> &);
    ~Wire();
    Wire(const Wire &)=delete;
    Message call(const QString &path,const QString &interface,const QString &method,const Append &append={},const char *signature="");
    Message waitSignal(const QString &path,const QString &interface,const QString &member,const char *signature,
        const std::function<bool(DBusMessage *)> &,int timeout=30000);
    bool live();
    void freeze();
    const LegacySourceBinding &binding() const {return binding_;}
private:
    Message raw(const QString &destination,const QString &path,const QString &interface,const QString &method,const Append &,int timeout);
    QString daemonText(const QString &method,const QString &argument);
    quint64 daemonNumber(const QString &method,const QString &argument);
    void drain();
    DBusConnection *connection_=nullptr;
    LegacySourceBinding binding_;
    std::function<bool()> admitted_;
    QElapsedTimer budget_;
    int pidfd_=-1;bool retired_=false,frozen_=false;
    std::deque<Message> signals_;
};
LegacySnapshot readSecretService(Wire &,const QStringList &);
LegacySnapshot readKWallet(Wire &);
bool sameSnapshot(const LegacySnapshot &,const LegacySnapshot &);
}
