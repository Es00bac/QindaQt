// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <QVariantMap>
#include <functional>

namespace QindaQt::StatusNotifier {

// GUI-thread, per-item exported-menu binding. The monitor owns this object;
// its injected admission/revision functions outlive it. Wire decoding remains
// exclusively in the public shared dbusmenu client (ADR-0114).
class StatusNotifierItemMenu final : public QObject {
    Q_OBJECT
public:
    using Admission = std::function<RegistryOutcome(const QString &identity)>;
    using NextRevision = std::function<quint64()>;
    StatusNotifierItemMenu(QDBusConnection connection, OwnerKey key, int timeout,
                           Admission admission, NextRevision nextRevision,
                           QObject *parent = nullptr);
    ~StatusNotifierItemMenu() override;
    void update(const ItemWireDetails &wire, const QString &identity);
    void invalidateDescriptor();
    void rejectDescriptor();
    [[nodiscard]] bool itemIsMenu() const;
    [[nodiscard]] bool descriptorCurrent() const { return m_descriptorCurrent; }
    [[nodiscard]] bool hasExportedMenu() const;
    [[nodiscard]] QVariantMap state() const;
    [[nodiscard]] RegistryOutcome open();
    [[nodiscard]] RegistryOutcome aboutToShow(quint64 revision, int id);
    [[nodiscard]] RegistryOutcome invoke(quint64 revision, int id);

signals:
    void changed();

private:
    void retire();
    void start();
    void publish(bool changedContents = true);
    [[nodiscard]] RegistryOutcome validate(quint64 revision, int id, bool submenu) const;
    QDBusConnection m_connection;
    OwnerKey m_key;
    int m_timeout;
    Admission m_admission;
    NextRevision m_nextRevision;
    Shell::GlobalMenu::DbusMenu::DbusMenuClient *m_client = nullptr;
    ItemWireDetails m_wire;
    QString m_identity;
    QString m_status = QStringLiteral("none");
    QVariantList m_entries;
    quint64 m_revision = 0;
    bool m_descriptorCurrent = false;
    bool m_openRequested = false;
};
} // namespace QindaQt::StatusNotifier
