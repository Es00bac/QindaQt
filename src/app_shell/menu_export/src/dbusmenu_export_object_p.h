// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QHash>
#include <QObject>
#include <QtDBus/QDBusVariant>

#include <optional>

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::AppShell::MenuExport {

class DbusMenuExportObject final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
  Q_PROPERTY(quint32 Version READ version SCRIPTABLE true)
  Q_PROPERTY(QString Status READ protocolStatus SCRIPTABLE true)
  Q_PROPERTY(QString TextDirection READ textDirection SCRIPTABLE true)

public:
  explicit DbusMenuExportObject(ApplicationCoordinator &coordinator,
                                QObject *parent = nullptr);

  [[nodiscard]] quint32 version() const noexcept;
  [[nodiscard]] QString protocolStatus() const;
  [[nodiscard]] QString textDirection() const;
  void publish(const Shell::GlobalMenu::Protocol::MenuTree &tree,
               const QHash<qint32, QString> &actionsByTransportId);

public Q_SLOTS:
  Q_SCRIPTABLE void
  GetLayout(qint32 parentId, qint32 recursionDepth,
            const QStringList &propertyNames, quint32 &revision,
            Shell::GlobalMenu::DbusMenu::LayoutItem &layout) const;
  Q_SCRIPTABLE Shell::GlobalMenu::DbusMenu::PropertyEntryList
  GetGroupProperties(const QList<qint32> &ids,
                     const QStringList &propertyNames) const;
  Q_SCRIPTABLE bool AboutToShow(qint32 itemId) const;
  Q_SCRIPTABLE void Event(qint32 itemId, const QString &eventId,
                          const QDBusVariant &data, quint32 timestamp);

Q_SIGNALS:
  Q_SCRIPTABLE void LayoutUpdated(quint32 revision, qint32 parentId);
  Q_SCRIPTABLE void ItemsPropertiesUpdated(
      Shell::GlobalMenu::DbusMenu::PropertyEntryList updated,
      Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList removed);

private:
  [[nodiscard]] static Shell::GlobalMenu::DbusMenu::LayoutItem
  encodeNode(const Shell::GlobalMenu::Protocol::MenuItem &item);
  [[nodiscard]] static QVariantMap
  propertiesFor(const Shell::GlobalMenu::Protocol::MenuItem &item);
  [[nodiscard]] std::optional<Shell::GlobalMenu::DbusMenu::LayoutItem>
  find(qint32 itemId) const;

  ApplicationCoordinator &m_coordinator;
  Shell::GlobalMenu::DbusMenu::LayoutItem m_layout;
  QHash<qint32, QString> m_actionsByTransportId;
  quint32 m_revision = 0;
};

} // namespace QindaQt::AppShell::MenuExport
