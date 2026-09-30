// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_wire_p.h"
#include <algorithm>
#include <cstring>
namespace qindaqt::keyring::importer {
namespace {[[noreturn]] void invalid(){throw Failure{CollectionImportError::InvalidInput};}}
void appendText(DBusMessageIter &iter,const QString &value,int type) {
    const auto utf8=value.toUtf8();const char *pointer=utf8.constData();
    if(value.contains(QChar(0)) || !dbus_message_iter_append_basic(&iter,type,&pointer)) throw Failure{CollectionImportError::Unavailable};
}
void appendInt(DBusMessageIter &iter,int value) {
    const dbus_int32_t number=value;
    if(!dbus_message_iter_append_basic(&iter,DBUS_TYPE_INT32,&number)) throw Failure{CollectionImportError::Unavailable};
}
void appendPaths(DBusMessageIter &iter,const QStringList &paths) {
    DBusMessageIter list;
    if(!dbus_message_iter_open_container(&iter,DBUS_TYPE_ARRAY,"o",&list)) throw Failure{CollectionImportError::Unavailable};
    for(const auto &path:paths) appendText(list,path,DBUS_TYPE_OBJECT_PATH);
    if(!dbus_message_iter_close_container(&iter,&list)) throw Failure{CollectionImportError::Unavailable};
}
DBusMessageIter begin(DBusMessage *message) {DBusMessageIter iter;dbus_message_iter_init(message,&iter);return iter;}
DBusMessageIter child(DBusMessageIter &iter,int expected) {
    if(dbus_message_iter_get_arg_type(&iter)!=expected) invalid();
    DBusMessageIter result;dbus_message_iter_recurse(&iter,&result);dbus_message_iter_next(&iter);return result;
}
void end(DBusMessageIter &iter) {if(dbus_message_iter_get_arg_type(&iter)!=DBUS_TYPE_INVALID) invalid();}
QString text(DBusMessageIter &iter,int type,qsizetype maximum) {
    if(dbus_message_iter_get_arg_type(&iter)!=type) invalid();
    const char *value=nullptr;dbus_message_iter_get_basic(&iter,&value);
    if(!value || std::strlen(value)>static_cast<std::size_t>(maximum)) invalid();
    const auto decoded=QString::fromUtf8(value);
    if(decoded.toUtf8()!=value) invalid();
    dbus_message_iter_next(&iter);return decoded;
}
std::uint64_t unsigned64(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter)!=DBUS_TYPE_UINT64) invalid();
    dbus_uint64_t value=0;dbus_message_iter_get_basic(&iter,&value);dbus_message_iter_next(&iter);return value;
}
int integer(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter)!=DBUS_TYPE_INT32) invalid();
    dbus_int32_t value=0;dbus_message_iter_get_basic(&iter,&value);dbus_message_iter_next(&iter);return value;
}
bool boolean(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter)!=DBUS_TYPE_BOOLEAN) invalid();
    dbus_bool_t value=0;dbus_message_iter_get_basic(&iter,&value);dbus_message_iter_next(&iter);return value!=0;
}
QStringList texts(DBusMessageIter &iter,int type,qsizetype maximum) {
    auto list=child(iter,DBUS_TYPE_ARRAY);
    QStringList result;
    while(dbus_message_iter_get_arg_type(&list)!=DBUS_TYPE_INVALID) {
        if(result.size()>=maximum) throw Failure{CollectionImportError::Capacity};
        const auto value=text(list,type);if(result.contains(value)) invalid();result.append(value);
    }
    return result;
}
SecureBuffer bytes(DBusMessageIter &iter,std::size_t maximum) {
    if(dbus_message_iter_get_arg_type(&iter)!=DBUS_TYPE_ARRAY || dbus_message_iter_get_element_type(&iter)!=DBUS_TYPE_BYTE) invalid();
    auto list=child(iter,DBUS_TYPE_ARRAY);const unsigned char *data=nullptr;int length=0;
    dbus_message_iter_get_fixed_array(&list,&data,&length);
    if(length<0 || static_cast<std::size_t>(length)>maximum) throw Failure{CollectionImportError::Capacity};
    SecureBuffer result(static_cast<std::size_t>(length));
    if(length) std::copy_n(data,length,result.bytes().begin());
    // Primary libdbus contract makes data borrowed/const. Dispose the owning
    // message promptly; never cast away const or claim its heap was wiped.
    return result;
}
Attributes stringMap(DBusMessageIter &iter) {
    auto list=child(iter,DBUS_TYPE_ARRAY);Attributes result;
    while(dbus_message_iter_get_arg_type(&list)!=DBUS_TYPE_INVALID) {
        if(result.size()>=32) throw Failure{CollectionImportError::Capacity};
        auto entry=child(list,DBUS_TYPE_DICT_ENTRY);const auto key=text(entry).toStdString();const auto value=text(entry).toStdString();end(entry);
        if(key.empty() || !result.emplace(key,value).second) invalid();
    }
    return result;
}
}
