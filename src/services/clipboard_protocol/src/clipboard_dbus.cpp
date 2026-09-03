// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_protocol/clipboard_dbus.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Services::ClipboardModel {

QDBusArgument &operator<<(QDBusArgument &argument, const EntryId &value)
{
    argument.beginStructure();
    argument << value.generation << value.serial;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, EntryId &value)
{
    value = {};
    argument.beginStructure();
    argument >> value.generation >> value.serial;
    argument.endStructure();
    return argument;
}

} // namespace QindaQt::Services::ClipboardModel

namespace QindaQt::Services::Clipboard {

void registerDBusTypes()
{
    qRegisterMetaType<ClipboardModel::EntryId>();
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<OperationResult>();
    qDBusRegisterMetaType<ClipboardModel::EntryId>();
    qDBusRegisterMetaType<Snapshot>();
    qDBusRegisterMetaType<OperationResult>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value)
{
    argument.beginStructure();
    argument << value.schemaVersion << value.epoch << value.generation << value.revision
             << value.historyEnabled << value.privacyAllowed << value.descriptorList;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value)
{
    value = {};
    argument.beginStructure();
    argument >> value.schemaVersion >> value.epoch >> value.generation >> value.revision
        >> value.historyEnabled >> value.privacyAllowed >> value.descriptorList;
    argument.endStructure();
    value.wireValid = argument.currentType() == QDBusArgument::UnknownType;
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value)
{
    argument.beginStructure();
    argument << static_cast<quint32>(value.kind) << static_cast<quint32>(value.status)
             << value.requestId << value.initiatingEpoch << value.initiatingGeneration
             << value.initiatingRevision << value.observedEpoch << value.observedGeneration
             << value.observedRevision << value.reasonCode;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value)
{
    value = {};
    quint32 kind = 0;
    quint32 status = 0;
    argument.beginStructure();
    argument >> kind >> status >> value.requestId >> value.initiatingEpoch
        >> value.initiatingGeneration >> value.initiatingRevision >> value.observedEpoch
        >> value.observedGeneration >> value.observedRevision >> value.reasonCode;
    argument.endStructure();
    value.kind = static_cast<OperationKind>(kind);
    value.status = static_cast<OperationStatus>(status);
    value.wireValid = argument.currentType() == QDBusArgument::UnknownType;
    return argument;
}

} // namespace QindaQt::Services::Clipboard
