// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QJsonObject>
#include <QObject>
#include <optional>

namespace QindaQt::Apps::SettingsInput {
// Borrowed, GUI-thread asynchronous port. Consumers own their selected row;
// snapshots replace all truth. Completed mutations require fresh readback.
class ControllerPort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void refresh() = 0;
    virtual void apply(const QString &id, const QJsonObject &patch, quint64 revision) = 0;
    virtual void reset(const QString &id, quint64 revision) = 0;
Q_SIGNALS:
    void snapshotReceived(const QJsonObject &snapshot);
    void completed(bool ok, const QString &reason);
    void unavailable();
};
class QtControllerPort final : public ControllerPort {
    Q_OBJECT
public:
    explicit QtControllerPort(QDBusConnection bus, QObject *parent = nullptr);
    void refresh() override;
    void apply(const QString &id, const QJsonObject &patch, quint64 revision) override;
    void reset(const QString &id, quint64 revision) override;
private Q_SLOTS:
    void changed(qulonglong revision);
private:
    void mutate(const QString &method, const QVariantList &arguments);
    QDBusConnection m_bus;
    quint64 m_generation = 1;
    bool m_refreshing = false, m_refreshAgain = false;
    std::optional<QPair<bool, QString>> m_completion;
};
} // namespace QindaQt::Apps::SettingsInput
