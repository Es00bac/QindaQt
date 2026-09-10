// SPDX-License-Identifier: GPL-3.0-or-later
#include "taskorderpersistence.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <QDebug>

namespace QindaQt::Shell {

TaskOrderPersistence::TaskOrderPersistence(
    Services::SettingsClient::SettingsClient &settings,
    ShellTaskListApplet::TaskListAppletController &controller, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_controller(controller)
{
    connect(&m_controller,
            &ShellTaskListApplet::TaskListAppletController::taskOrderCommitted,
            this, &TaskOrderPersistence::persistOrder);
    connect(&m_settings,
            &Services::SettingsClient::SettingsClient::commitUncertain, this,
            [](const QString &message) {
                qWarning().noquote()
                    << "QindaQt shell could not persist the task order:"
                    << message;
            });
}

void TaskOrderPersistence::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    connect(&m_settings,
            &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &TaskOrderPersistence::decodeFromSnapshot);
    decodeFromSnapshot();
}

std::optional<QStringList> TaskOrderPersistence::decodeTaskOrder(
    const QVariant &panelsConfiguration)
{
    if (!panelsConfiguration.isValid() || panelsConfiguration.isNull()) {
        return QStringList{};
    }
    if (panelsConfiguration.metaType().id() != QMetaType::QVariantMap) {
        return std::nullopt;
    }
    const QVariant order =
        panelsConfiguration.toMap().value(QLatin1String(kOrderField));
    if (!order.isValid() || order.isNull()) {
        return QStringList{};
    }
    const auto type = order.metaType().id();
    if (type != QMetaType::QStringList && type != QMetaType::QVariantList) {
        return std::nullopt;
    }
    QStringList ids;
    const QVariantList list = order.toList();
    ids.reserve(list.size());
    for (const QVariant &entry : list) {
        if (entry.metaType().id() != QMetaType::QString) {
            return std::nullopt;
        }
        ids.append(entry.toString());
    }
    return ids;
}

QVariantMap TaskOrderPersistence::mergeTaskOrder(const QVariant &current,
                                                 const QStringList &order)
{
    QVariantMap merged;
    if (current.metaType().id() == QMetaType::QVariantMap) {
        merged = current.toMap();
    }
    merged.insert(QLatin1String(kOrderField), order);
    return merged;
}

void TaskOrderPersistence::decodeFromSnapshot()
{
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot) {
        return;
    }
    const std::optional<QStringList> decoded =
        decodeTaskOrder(snapshot->values.value(QLatin1String(kKey)));
    if (!decoded.has_value()) {
        qWarning().noquote()
            << "QindaQt shell ignored a malformed panels.configuration "
               "task order; the canonical order stays in effect";
        return;
    }
    m_controller.setUserTaskOrder(*decoded);
}

void TaskOrderPersistence::persistOrder(const QStringList &orderedIds)
{
    const auto &snapshot = m_settings.snapshot();
    const QVariant current = snapshot
        ? snapshot->values.value(QLatin1String(kKey))
        : QVariant{};
    const QVariantMap merged = mergeTaskOrder(current, orderedIds);
    if (merged == current.toMap()) {
        return;
    }
    QString error;
    if (!m_settings.setUserValue(QLatin1String(kKey), merged, &error)) {
        qWarning().noquote()
            << "QindaQt shell could not persist the task order:" << error;
    }
}

} // namespace QindaQt::Shell
