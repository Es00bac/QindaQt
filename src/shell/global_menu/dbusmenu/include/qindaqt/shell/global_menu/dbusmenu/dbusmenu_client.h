// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_decoder.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusObjectPath>

#include <optional>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

struct ClientMetadata final {
    quint32 version = 0;
    QString status;
    QString textDirection;
    bool valid = false;

    bool operator==(const ClientMetadata &) const = default;
};

// Exact-owner asynchronous com.canonical.dbusmenu adapter. Layout and
// property signals are invalidation hints; only a complete GetLayout reply can
// replace the accepted snapshot. Requests are fenced by owner generation and
// serial, and Event is sent once with no retry after an uncertain outcome.
// It owns all pending-call/watch objects and copied snapshots; the injected
// connection handle, this QObject, and its public calls stay on one Qt thread.
// start() reports configuration/subscription failure and wire failures retain
// the last accepted snapshot while emitting a bounded reason code.
class DbusMenuClient final : public QObject, public Exporter::MenuSource
{
    Q_OBJECT

public:
    DbusMenuClient(QDBusConnection connection, QString ownerUniqueName,
                   QDBusObjectPath objectPath, QUuid ownerWindowId,
                   int timeoutMilliseconds = 2'000, QObject *parent = nullptr);
    ~DbusMenuClient() override;

    [[nodiscard]] bool start(QString *error = nullptr);
    void stop();
    [[nodiscard]] bool isStarted() const noexcept;
    [[nodiscard]] Exporter::MenuSnapshot snapshot() const override;
    [[nodiscard]] ClientMetadata metadata() const;
    [[nodiscard]] quint32 remoteRevision() const noexcept;

    void refreshLayout();
    void requestGroupProperties(const QList<qint32> &itemIds);
    void aboutToShow(qint32 itemId);
    void sendEvent(qint32 itemId, const QString &eventId,
                   const QVariant &data = {}, quint32 timestamp = 0);

Q_SIGNALS:
    void treeChanged();
    void metadataChanged();
    void unavailable();
    void rejected(QString reasonCode);
    void eventCompleted(qint32 itemId, bool delivered, QString reasonCode);

private Q_SLOTS:
    void layoutUpdated(quint32 revision, qint32 parentId);
    void itemsPropertiesUpdated(
        QindaQt::Shell::GlobalMenu::DbusMenu::PropertyEntryList updated,
        QindaQt::Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList removed);
    void propertiesChanged(QString interfaceName, QVariantMap changed,
                           QStringList invalidated);

private:
    void requestMetadata();
    void disconnectSignals();
    void retirePendingCalls();
    void handleOwnerLoss();

    QDBusConnection m_connection;
    QString m_ownerUniqueName;
    QDBusObjectPath m_objectPath;
    QUuid m_ownerWindowId;
    int m_timeoutMilliseconds = 2'000;
    QDBusServiceWatcher *m_ownerWatcher = nullptr;
    QList<QDBusPendingCallWatcher *> m_pendingCalls;
    std::optional<Exporter::MenuSnapshot> m_snapshot;
    ClientMetadata m_metadata;
    quint64 m_generation = 0;
    quint64 m_layoutSerial = 0;
    quint32 m_remoteRevision = 0;
    bool m_started = false;
    bool m_layoutInFlight = false;
    bool m_layoutDirty = false;
};

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
