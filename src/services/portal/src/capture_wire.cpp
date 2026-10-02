// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/capture_types.h>
#include <QDBusMetaType>
namespace QindaQt::Services::Portal {
QDBusArgument &operator<<(QDBusArgument &a, const CaptureColor &v) { a.beginStructure(); a << v.red << v.green << v.blue; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, CaptureColor &v) { a.beginStructure(); a >> v.red >> v.green >> v.blue; a.endStructure(); return a; }
QDBusArgument &operator<<(QDBusArgument &a, const CaptureCoordinate &v) { a.beginStructure(); a << v.first << v.second; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, CaptureCoordinate &v) { a.beginStructure(); a >> v.first >> v.second; a.endStructure(); return a; }
QDBusArgument &operator<<(QDBusArgument &a, const CaptureStream &v) { a.beginStructure(); a << v.node << v.properties; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, CaptureStream &v) { a.beginStructure(); a >> v.node >> v.properties; a.endStructure(); return a; }
QDBusArgument &operator<<(QDBusArgument &a, const CaptureRestoreData &v) { a.beginStructure(); a << v.vendor << v.version << v.payload; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, CaptureRestoreData &v) { a.beginStructure(); a >> v.vendor >> v.version >> v.payload; a.endStructure(); return a; }
void registerCaptureWireTypes() { qDBusRegisterMetaType<CaptureColor>(); qDBusRegisterMetaType<CaptureCoordinate>(); qDBusRegisterMetaType<CaptureStream>(); qDBusRegisterMetaType<CaptureStreams>(); qDBusRegisterMetaType<CaptureRestoreData>(); }
}
