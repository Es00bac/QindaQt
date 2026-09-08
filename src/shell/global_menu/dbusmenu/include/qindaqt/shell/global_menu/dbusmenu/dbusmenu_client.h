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
// the last accepted snapshot while emitting a bounded reason code; a failed
// first GetLayout additionally emits initialLayoutFailed() so the consumer can
// distinguish never-delivered from temporarily-undeliverable.
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
    // False during layout/AboutToShow requests and after a failed read. The
    // retained snapshot may still be displayed, but current-action consumers
    // must refuse dispatch until a complete accepted read makes it true.
    [[nodiscard]] bool isLayoutCurrent() const noexcept;

    void refreshLayout();
    void requestGroupProperties(const QList<qint32> &itemIds);
    void aboutToShow(qint32 itemId);
    void sendEvent(qint32 itemId, const QString &eventId,
                   const QVariant &data = {}, quint32 timestamp = 0);

Q_SIGNALS:
    void treeChanged();
    void layoutCurrentChanged();
    void metadataChanged();
    void unavailable();
    void rejected(QString reasonCode);
    // Emitted only when a GetLayout reply errors or fails to decode while this
    // client has never accepted a snapshot: the endpoint has never delivered a
    // menu. Unlike `rejected`, which also covers mid-session re-read failures
    // that must retain the last accepted snapshot, this tells the consumer the
    // binding can never produce content and should be torn down rather than
    // left waiting.
    void initialLayoutFailed(QString reasonCode);
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
    void setLayoutCurrent(bool current);
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
    bool m_layoutCurrent = false;
    bool m_layoutValid = false;
    int m_aboutToShowPending = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
