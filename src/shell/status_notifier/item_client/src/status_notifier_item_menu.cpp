// SPDX-License-Identifier: LGPL-3.0-or-later
#include "status_notifier_item_menu.h"

#include <QPointer>

namespace QindaQt::StatusNotifier {
namespace {
using Shell::GlobalMenu::Protocol::MenuItem;
using Shell::GlobalMenu::Protocol::MenuItemKind;
using Shell::GlobalMenu::DbusMenu::DbusMenuClient;

QVariantList project(const QList<MenuItem> &items, bool ancestorsEnabled = true) {
    QVariantList result;
    for (const auto &item : items) {
        if (!item.visible) {
            continue;
        }
        const bool enabled = ancestorsEnabled && item.enabled;
        const QString kind = item.kind == MenuItemKind::Submenu ? QStringLiteral("submenu")
            : item.kind == MenuItemKind::Separator ? QStringLiteral("separator")
                                                   : QStringLiteral("action");
        result.append(QVariantMap{{QStringLiteral("id"), item.id.toInt()},
                                  {QStringLiteral("kind"), kind},
                                  {QStringLiteral("label"), item.text},
                                  {QStringLiteral("enabled"), enabled},
                                  {QStringLiteral("separator"), item.kind == MenuItemKind::Separator},
                                  {QStringLiteral("checkable"), item.checkable},
                                  {QStringLiteral("checked"), item.checked},
                                  {QStringLiteral("radio"), !item.radioGroup.isEmpty()},
                                  {QStringLiteral("iconName"), QString{}},
                                  {QStringLiteral("shortcut"), item.shortcutText},
                                  {QStringLiteral("children"), project(item.children, enabled)}});
    }
    return result;
}

const MenuItem *findAction(const QList<MenuItem> &items, const QString &id) {
    for (const auto &item : items) {
        if (!item.visible || !item.enabled) {
            continue;
        }
        if (item.id == id) {
            return &item;
        }
        if (const auto *found = findAction(item.children, id)) {
            return found;
        }
    }
    return nullptr;
}
} // namespace

StatusNotifierItemMenu::StatusNotifierItemMenu(QDBusConnection connection, OwnerKey key,
                                               int timeout, Admission admission,
                                               NextRevision nextRevision, QObject *parent)
    : QObject(parent), m_connection(std::move(connection)), m_key(std::move(key)),
      m_timeout(timeout), m_admission(std::move(admission)),
      m_nextRevision(std::move(nextRevision)) {}

StatusNotifierItemMenu::~StatusNotifierItemMenu() { retire(); }

void StatusNotifierItemMenu::retire() {
    if (m_client) {
        m_client->disconnect(this);
    }
    delete m_client;
    m_client = nullptr;
    m_entries.clear();
}

void StatusNotifierItemMenu::update(const ItemWireDetails &wire, const QString &identity) {
    const bool replaced = !m_descriptorCurrent || m_wire.menuObjectPath != wire.menuObjectPath
        || m_wire.itemIsMenu != wire.itemIsMenu || m_identity != identity;
    if (!replaced) {
        return;
    }
    retire();
    m_wire = wire;
    m_identity = identity;
    m_descriptorCurrent = true;
    m_status = hasExportedMenu() ? QStringLiteral("loading") : QStringLiteral("none");
    if (m_openRequested && hasExportedMenu()) {
        start();
    }
    publish();
}

void StatusNotifierItemMenu::invalidateDescriptor() {
    m_descriptorCurrent = false;
    retire();
    m_status = QStringLiteral("loading");
    publish();
}

void StatusNotifierItemMenu::rejectDescriptor() {
    m_descriptorCurrent = false;
    retire();
    m_status = QStringLiteral("error");
    publish();
}

bool StatusNotifierItemMenu::itemIsMenu() const {
    return m_wire.itemIsMenu;
}

bool StatusNotifierItemMenu::hasExportedMenu() const {
    // Qt advertises this sentinel when QSystemTrayIcon has no QMenu.
    return !m_wire.menuObjectPath.isEmpty()
        && m_wire.menuObjectPath != QStringLiteral("/NO_DBUSMENU");
}

QVariantMap StatusNotifierItemMenu::state() const {
    return {{QStringLiteral("status"), m_status},
            {QStringLiteral("revision"), QString::number(m_revision)},
            {QStringLiteral("entries"), m_entries}};
}

void StatusNotifierItemMenu::publish(bool changedContents) {
    if (changedContents) {
        m_revision = m_nextRevision();
    }
    if (m_revision == 0) {
        m_status = QStringLiteral("error");
    }
    emit changed();
}

void StatusNotifierItemMenu::start() {
    m_client = new DbusMenuClient(m_connection, m_key.uniqueName,
        QDBusObjectPath(m_wire.menuObjectPath), QUuid::createUuid(), m_timeout, this);
    // AGENT-NOTE: The shared client's opaque UUID is only local decoder
    // lineage here; tray authority remains the registry OwnerKey, never a
    // fabricated compositor/window registration. See ADR-0114.
    connect(m_client, &DbusMenuClient::layoutCurrentChanged, this, [this] {
        const auto snapshot = m_client->snapshot();
        bool changedContents = false;
        if (m_client->isLayoutCurrent() && snapshot.complete) {
            const auto entries = project(snapshot.tree.items);
            changedContents = m_entries != entries;
            m_entries = entries;
            m_status = QStringLiteral("ready");
        } else {
            m_status = QStringLiteral("loading");
        }
        publish(changedContents);
    });
    connect(m_client, &DbusMenuClient::rejected, this, [this](const QString &) {
        if (!m_client->isLayoutCurrent()) {
            m_status = QStringLiteral("error");
            publish(false);
        }
    });
    connect(m_client, &DbusMenuClient::unavailable, this, [this] {
        m_entries.clear();
        m_status = QStringLiteral("error");
        publish();
    });
    if (!m_client->start()) {
        m_status = QStringLiteral("error");
        return;
    }
    m_client->aboutToShow(0);
}

RegistryOutcome StatusNotifierItemMenu::open() {
    const auto admission = m_admission(m_identity);
    if (!admission.accepted()) {
        return admission;
    }
    if (!m_descriptorCurrent || !hasExportedMenu()) {
        return {RegistryStatus::InvalidRequest, QStringLiteral("menu-not-available")};
    }
    m_openRequested = true;
    if (m_status == QStringLiteral("error")) {
        retire();
    }
    if (!m_client) {
        m_status = QStringLiteral("loading");
        start();
    } else if (m_client->isLayoutCurrent()) {
        m_client->aboutToShow(0);
    }
    publish(false);
    return {RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierItemMenu::validate(quint64 revision, int id, bool submenu) const {
    const auto admission = m_admission(m_identity);
    if (!admission.accepted()) {
        return admission;
    }
    if (!m_descriptorCurrent || !m_client || !m_client->isLayoutCurrent()
        || m_status != QStringLiteral("ready") || revision == 0 || revision != m_revision) {
        return {RegistryStatus::InvalidRequest, QStringLiteral("stale-menu-revision")};
    }
    const auto snapshot = m_client->snapshot();
    const auto *item = snapshot.complete ? findAction(snapshot.tree.items, QString::number(id)) : nullptr;
    if (id <= 0 || !item || item->kind != (submenu ? MenuItemKind::Submenu : MenuItemKind::Action)) {
        return {RegistryStatus::InvalidRequest, QStringLiteral("menu-action-unavailable")};
    }
    return {RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierItemMenu::aboutToShow(quint64 revision, int id) {
    const auto result = validate(revision, id, true);
    if (result.accepted()) {
        m_client->aboutToShow(id);
    }
    return result;
}

RegistryOutcome StatusNotifierItemMenu::invoke(quint64 revision, int id) {
    const auto result = validate(revision, id, false);
    if (!result.accepted()) {
        return result;
    }
    // Consume the exact revision before notifying presentation, so reentrant
    // activation of one captured menu intent cannot dispatch twice.
    m_revision = m_nextRevision();
    if (m_revision == 0) {
        m_status = QStringLiteral("error");
        emit changed();
        return {RegistryStatus::InvalidRequest, QStringLiteral("menu-revision-exhausted")};
    }
    m_client->sendEvent(id, QStringLiteral("clicked"));
    emit changed();
    return result;
}
} // namespace QindaQt::StatusNotifier
