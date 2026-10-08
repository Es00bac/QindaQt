// SPDX-License-Identifier: LGPL-3.0-or-later
#include "native_voice_codec_p.h"
#include <cstring>
namespace QindaQt::Services::VoiceConfiguration {
namespace {
bool text(DBusMessageIter *iter, QString *out, size_t bound) {
    const char *value = nullptr;
    dbus_message_iter_get_basic(iter, &value);
    if (!value || strnlen(value,bound + 1) > bound) return false;
    *out = QString::fromUtf8(value); return true;
}
bool scalar(DBusMessageIter *iter, QVariant *out) {
    switch (dbus_message_iter_get_arg_type(iter)) {
    case DBUS_TYPE_STRING: { QString value; if (!text(iter,&value,512)) return false; *out=value; return true; }
    case DBUS_TYPE_BOOLEAN: { dbus_bool_t value=0; dbus_message_iter_get_basic(iter,&value); *out=bool(value); return true; }
    case DBUS_TYPE_UINT32: { dbus_uint32_t value=0; dbus_message_iter_get_basic(iter,&value); *out=quint32(value); return true; }
    case DBUS_TYPE_UINT64: { dbus_uint64_t value=0; dbus_message_iter_get_basic(iter,&value); *out=QVariant::fromValue(quint64(value)); return true; }
    case DBUS_TYPE_INT32: { dbus_int32_t value=0; dbus_message_iter_get_basic(iter,&value); *out=qint32(value); return true; }
    case DBUS_TYPE_INT64: { dbus_int64_t value=0; dbus_message_iter_get_basic(iter,&value); *out=QVariant::fromValue(qint64(value)); return true; }
    default: return false;
    }
}
}
bool nativeMap(DBusMessage *message, QVariantMap *value) {
    if (!message || dbus_message_get_type(message)!=DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(message,"a{sv}")) return false;
    DBusMessageIter outer, array; dbus_message_iter_init(message,&outer);
    dbus_message_iter_recurse(&outer,&array); QVariantMap result;
    while (dbus_message_iter_get_arg_type(&array)!=DBUS_TYPE_INVALID) {
        if (result.size()>=10 || dbus_message_iter_get_arg_type(&array)!=DBUS_TYPE_DICT_ENTRY) return false;
        DBusMessageIter entry; dbus_message_iter_recurse(&array,&entry);
        if (dbus_message_iter_get_arg_type(&entry)!=DBUS_TYPE_STRING) return false;
        QString key; if (!text(&entry,&key,64) || result.contains(key)) return false;
        if (!dbus_message_iter_next(&entry) || dbus_message_iter_get_arg_type(&entry)!=DBUS_TYPE_VARIANT) return false;
        DBusMessageIter variant; dbus_message_iter_recurse(&entry,&variant);
        QVariant field; if (!scalar(&variant,&field) || dbus_message_iter_next(&variant)
            || dbus_message_iter_next(&entry)) return false;
        result.insert(key,field); dbus_message_iter_next(&array);
    }
    *value=result; return true;
}
bool appendArguments(DBusMessage *message, const QVariantList &arguments) {
    if (!message || arguments.size()>3) return false;
    DBusMessageIter iter; dbus_message_iter_init_append(message,&iter);
    for (const QVariant &value : arguments) {
        if (value.metaType().id()==QMetaType::ULongLong) {
            dbus_uint64_t number=value.toULongLong();
            if (!dbus_message_iter_append_basic(&iter,DBUS_TYPE_UINT64,&number)) return false;
        } else if (value.metaType().id()==QMetaType::QString) {
            const QByteArray bytes=value.toString().toUtf8(); const char *text=bytes.constData();
            if (bytes.size()>512 || !dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&text)) return false;
        } else return false;
    }
    return true;
}
}
