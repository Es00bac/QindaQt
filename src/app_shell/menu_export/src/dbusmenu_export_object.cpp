// SPDX-License-Identifier: LGPL-3.0-or-later
#include "dbusmenu_export_object_p.h"

#include <qindaqt/app_shell/application_coordinator.h>

#include <limits>

namespace QindaQt::AppShell::MenuExport {
namespace {

namespace DbusMenu = Shell::GlobalMenu::DbusMenu;
namespace Protocol = Shell::GlobalMenu::Protocol;
using DbusMenu::LayoutItem;
using Protocol::MenuItem;
using Protocol::MenuItemKind;

qint32 transportId(const QString &id) {
  bool ok = false;
  const qint32 value = id.toInt(&ok);
  return ok && value > 0 && QString::number(value) == id ? value : 0;
}

Shell::GlobalMenu::DbusMenu::ShortcutList
shortcutList(const QString &portableShortcut) {
  if (portableShortcut.isEmpty()) {
    return {};
  }
  DbusMenu::ShortcutList sequences;
  for (const QString &sequence : portableShortcut.split(QLatin1Char(','))) {
    QStringList tokens = sequence.trimmed().split(QLatin1Char('+'));
    for (QString &token : tokens) {
      if (token == QStringLiteral("Ctrl")) {
        token = QStringLiteral("Control");
      } else if (token == QStringLiteral("Meta")) {
        token = QStringLiteral("Super");
      }
    }
    sequences.append(tokens);
  }
  return sequences;
}

std::optional<LayoutItem> findNode(const LayoutItem &node, qint32 itemId) {
  if (node.id == itemId) {
    return node;
  }
  for (const QVariant &childValue : node.children) {
    if (!childValue.canConvert<LayoutItem>()) {
      continue;
    }
    const LayoutItem child = childValue.value<LayoutItem>();
    if (std::optional<LayoutItem> found = findNode(child, itemId)) {
      return found;
    }
  }
  return std::nullopt;
}

} // namespace

DbusMenuExportObject::DbusMenuExportObject(ApplicationCoordinator &coordinator,
                                           QObject *parent)
    : QObject(parent), m_coordinator(coordinator) {
  DbusMenu::registerDbusMenuWireTypes();
  // moc records these types relative to the enclosing QindaQt namespace.
  // Register those exact spellings before exporting the adaptor object.
  qRegisterMetaType<DbusMenu::LayoutItem>(
      "Shell::GlobalMenu::DbusMenu::LayoutItem");
  qRegisterMetaType<DbusMenu::PropertyEntryList>(
      "Shell::GlobalMenu::DbusMenu::PropertyEntryList");
  qRegisterMetaType<DbusMenu::RemovedPropertyEntryList>(
      "Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList");
}

quint32 DbusMenuExportObject::version() const noexcept { return 4; }

QString DbusMenuExportObject::protocolStatus() const {
  return QStringLiteral("normal");
}

QString DbusMenuExportObject::textDirection() const {
  return QStringLiteral("ltr");
}

void DbusMenuExportObject::publish(
    const Shell::GlobalMenu::Protocol::MenuTree &tree,
    const QHash<qint32, QString> &actionsByTransportId) {
  LayoutItem root{.id = 0, .properties = {}, .children = {}};
  root.children.reserve(tree.items.size());
  for (const MenuItem &item : tree.items) {
    root.children.append(QVariant::fromValue(encodeNode(item)));
  }
  if (m_revision == std::numeric_limits<quint32>::max()) {
    return;
  }
  m_layout = std::move(root);
  m_actionsByTransportId = actionsByTransportId;
  ++m_revision;
  Q_EMIT LayoutUpdated(m_revision, 0);
}

void DbusMenuExportObject::GetLayout(qint32 parentId, qint32,
                                     const QStringList &, quint32 &revision,
                                     LayoutItem &layout) const {
  revision = m_revision;
  layout = find(parentId).value_or(LayoutItem{});
}

Shell::GlobalMenu::DbusMenu::PropertyEntryList
DbusMenuExportObject::GetGroupProperties(const QList<qint32> &ids,
                                         const QStringList &) const {
  Shell::GlobalMenu::DbusMenu::PropertyEntryList result;
  result.reserve(ids.size());
  for (qint32 id : ids) {
    if (const std::optional<LayoutItem> item = find(id)) {
      result.append({.id = id, .properties = item->properties});
    }
  }
  return result;
}

bool DbusMenuExportObject::AboutToShow(qint32 itemId) const {
  return find(itemId).has_value();
}

void DbusMenuExportObject::Event(qint32 itemId, const QString &eventId,
                                 const QDBusVariant &, quint32) {
  if (eventId != QStringLiteral("clicked")) {
    return;
  }
  const auto action = m_actionsByTransportId.constFind(itemId);
  if (action == m_actionsByTransportId.cend()) {
    return;
  }
  // AGENT-GUARD: one admitted dbusmenu Event crosses exactly once through
  // the same ActionRegistry gate as the in-window MenuBar. Never invoke an
  // application callback directly or retry after this point.
  (void)m_coordinator.activateAction(*action);
}

LayoutItem DbusMenuExportObject::encodeNode(const MenuItem &item) {
  LayoutItem encoded{.id = transportId(item.id),
                     .properties = propertiesFor(item),
                     .children = {}};
  encoded.children.reserve(item.children.size());
  for (const MenuItem &child : item.children) {
    encoded.children.append(QVariant::fromValue(encodeNode(child)));
  }
  return encoded;
}

QVariantMap DbusMenuExportObject::propertiesFor(const MenuItem &item) {
  if (item.kind == MenuItemKind::Separator) {
    return {{QStringLiteral("type"), QStringLiteral("separator")}};
  }
  QVariantMap result{{QStringLiteral("label"), item.text},
                     {QStringLiteral("enabled"), item.enabled},
                     {QStringLiteral("visible"), item.visible}};
  if (item.kind == MenuItemKind::Submenu) {
    result.insert(QStringLiteral("children-display"),
                  QStringLiteral("submenu"));
  }
  if (!item.shortcutText.isEmpty()) {
    result.insert(QStringLiteral("shortcut"),
                  QVariant::fromValue(shortcutList(item.shortcutText)));
  }
  if (item.checkable) {
    result.insert(QStringLiteral("toggle-type"), QStringLiteral("checkmark"));
    result.insert(QStringLiteral("toggle-state"), item.checked ? 1 : 0);
  }
  return result;
}

std::optional<LayoutItem> DbusMenuExportObject::find(qint32 itemId) const {
  return findNode(m_layout, itemId);
}

} // namespace QindaQt::AppShell::MenuExport
