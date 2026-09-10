// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <optional>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}

namespace QindaQt::Shell {

// AGENT-CONTRACT: one-way bridge between the shell's shared Settings1 client
// and the task-list controller's user-order overlay
// (ADR-0118). Both collaborators are borrowed and must outlive this object;
// everything runs GUI-thread confined. Settings snapshots feed
// controller.setUserTaskOrder (an exact no-op on echo, so the write path
// cannot loop); user drags arrive on controller.taskOrderCommitted and are
// merged into the panels.configuration object value. A commit that finishes
// Uncertain is logged and never replayed — Settings1 owns the durable truth
// and the in-memory order stays authoritative for the session.
class TaskOrderPersistence final : public QObject {
    Q_OBJECT

public:
    static constexpr QLatin1StringView kKey{"panels.configuration"};
    static constexpr QLatin1StringView kOrderField{"taskOrder"};

    TaskOrderPersistence(Services::SettingsClient::SettingsClient &settings,
                         ShellTaskListApplet::TaskListAppletController &controller,
                         QObject *parent = nullptr);

    void start();

    // Pure total decode of the persisted task order out of one
    // panels.configuration value. Absent value/field decodes to an empty
    // list; a present-but-malformed container or a non-string id returns
    // nullopt so the caller can reject the whole value instead of trusting a
    // partial order.
    [[nodiscard]] static std::optional<QStringList>
    decodeTaskOrder(const QVariant &panelsConfiguration);

    // Read-modify-write merge for the commit path: replaces only the task
    // order field and preserves every unrelated key of the current object.
    [[nodiscard]] static QVariantMap
    mergeTaskOrder(const QVariant &current, const QStringList &order);

private:
    void decodeFromSnapshot();
    void persistOrder(const QStringList &orderedIds);

    Services::SettingsClient::SettingsClient &m_settings;
    ShellTaskListApplet::TaskListAppletController &m_controller;
    bool m_started = false;
};

} // namespace QindaQt::Shell
