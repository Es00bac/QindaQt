// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QJsonObject>
#include <QKeySequence>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::Portal {
struct PortalShortcut { QString id; QVariantMap options; };
using PortalShortcuts = QList<PortalShortcut>;
struct ShortcutDraft { QString id, description; QKeySequence key; };
using ShortcutDrafts = QList<ShortcutDraft>;
QDBusArgument &operator<<(QDBusArgument &, const PortalShortcut &);
const QDBusArgument &operator>>(const QDBusArgument &, PortalShortcut &);
void registerShortcutWire();
std::optional<ShortcutDrafts> shortcutDrafts(const PortalShortcuts &);
QJsonObject shortcutFrame(const QString &app, const QString &parent, const QString &component, const ShortcutDrafts &);
std::optional<ShortcutDrafts> shortcutDraftsFromFrame(const QJsonObject &);
std::optional<ShortcutDrafts> shortcutSelection(const ShortcutDrafts &offered, const QJsonObject &);
PortalShortcuts shortcutDescriptions(const ShortcutDrafts &);
}
Q_DECLARE_METATYPE(QindaQt::Services::Portal::PortalShortcut)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::PortalShortcuts)
