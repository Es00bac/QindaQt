// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/chooser_types.h>
#include <QDBusMetaType>
namespace QindaQt::Services::Portal {
QDBusArgument &operator<<(QDBusArgument &a, const FilterRule &v) { a.beginStructure(); a << v.kind << v.pattern; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, FilterRule &v) { a.beginStructure(); a >> v.kind >> v.pattern; a.endStructure(); return a; }
QDBusArgument &operator<<(QDBusArgument &a, const FileFilter &v) { a.beginStructure(); a << v.label << v.rules; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, FileFilter &v) { a.beginStructure(); a >> v.label >> v.rules; a.endStructure(); return a; }
void registerChooserTypes() {
    registerAccessTypes();
    qDBusRegisterMetaType<FilterRule>(); qDBusRegisterMetaType<FilterRules>();
    qDBusRegisterMetaType<FileFilter>(); qDBusRegisterMetaType<FileFilters>();
    qDBusRegisterMetaType<FileNames>();
}
} // namespace QindaQt::Services::Portal
