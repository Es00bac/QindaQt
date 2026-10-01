// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_backend.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QQueue>
#include <QTimer>
#include <functional>
#include <optional>

namespace QindaQt::Apps::RemovableMedia {
class UDisksBackend final : public MediaBackend {
    Q_OBJECT
public:
    explicit UDisksBackend(QDBusConnection connection, QObject *parent = nullptr);
    QVector<Volume> volumes() const override { return m_volumes; }
    bool available() const override { return m_available; }
    QString diagnostic() const override { return m_diagnostic; }
    QStringList formatTypes() const override { return m_formatTypes; }
    void refresh() override;
    void execute(const Request &request) override;
private Q_SLOTS:
    void interfacesAdded(const QDBusObjectPath &path, const Interfaces &interfaces);
    void interfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void propertiesChanged(const QString &interface, const QVariantMap &properties,
                           const QStringList &invalidated, const QDBusMessage &message);
private:
    using ReplyHandler = std::function<void(const QDBusMessage &)>;
    struct Step { QString path, interface, method; QVariantList arguments; };
    void ownerChanged(const QString &owner);
    void fetch(std::function<void(bool)> continuation = {});
    void publish(const ManagedObjects &objects);
    void discoverFormats();
    void call(const Step &step, ReplyHandler handler);
    const Volume *find(const QString &token) const;
    void prepare(const Request &request, const Volume &expected);
    void runNext();
    void finish(bool success, const QString &message, const QString &mountPath = {});
    QDBusConnection m_bus;
    QDBusServiceWatcher m_watcher;
    QTimer m_debounce;
    QString m_owner, m_diagnostic, m_resultMount;
    QStringList m_formatTypes;
    QVector<Volume> m_volumes;
    ManagedObjects m_objects;
    QQueue<Step> m_steps;
    std::optional<Request> m_request;
    Volume m_expected;
    quint64 m_epoch = 0, m_attachment = 0, m_inventorySerial = 0;
    quint64 m_mutationSerial = 0;
    bool m_available = false;
    bool m_unlockedRemovalTransition = false;
};
} // namespace QindaQt::Apps::RemovableMedia
