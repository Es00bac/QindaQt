// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QJsonObject>
#include <QObject>
#include <optional>

namespace QindaQt::Apps::SettingsInput {
// Borrowed, GUI-thread asynchronous port. Consumers own their selected row;
// snapshots replace all truth. Completed mutations require fresh readback.
struct JsonInputEndpoint { QString service, object, interface; };
class JsonInputPort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void refresh() = 0;
    virtual void apply(const QString &id, const QJsonObject &patch, quint64 revision) = 0;
    virtual void reset(const QString &id, quint64 revision) = 0;
    virtual void run(const QString &, quint64) { Q_EMIT completed(false, "unsupported"); }
    virtual void cancelRun() {}
Q_SIGNALS:
    void snapshotReceived(const QJsonObject &snapshot);
    void completed(bool ok, const QString &reason);
    void unavailable();
};
class QtJsonInputPort final : public JsonInputPort {
    Q_OBJECT
public:
    explicit QtJsonInputPort(QDBusConnection bus, QObject *parent = nullptr, JsonInputEndpoint endpoint = {});
    void refresh() override;
    void apply(const QString &id, const QJsonObject &patch, quint64 revision) override;
    void reset(const QString &id, quint64 revision) override;
    void run(const QString &id, quint64 revision) override;
    void cancelRun() override;
private Q_SLOTS:
    void changed(qulonglong revision);
private:
    void resolveOwner();
    void mutate(const QString &method, const QVariantList &arguments);
    QDBusConnection m_bus;
    JsonInputEndpoint m_endpoint;
    QString m_owner;
    quint64 m_generation = 1;
    bool m_refreshing = false, m_refreshAgain = false;
    std::optional<QPair<bool, QString>> m_completion;
};
} // namespace QindaQt::Apps::SettingsInput
