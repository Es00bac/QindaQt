// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>

#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusVariant>

#include <utility>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{

namespace
{

constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

void setError(QString *target, QString value)
{
    if (target != nullptr) {
        *target = std::move(value);
    }
}

QDBusMessage methodCall(const QString &owner, const QDBusObjectPath &path, const char *interface,
                        const char *method)
{
    return QDBusMessage::createMethodCall(owner, path.path(), QString::fromLatin1(interface),
                                          QString::fromLatin1(method));
}

bool validMetadataMap(const QVariantMap &values, ClientMetadata *metadata)
{
    ClientMetadata candidate{.version = 0,
                             .status = QStringLiteral("normal"),
                             .textDirection = QStringLiteral("ltr"),
                             .valid = true};
    const auto version = values.constFind(QStringLiteral("Version"));
    if (version != values.cend()) {
        const QVariant value = version->metaType() == QMetaType::fromType<QDBusVariant>()
            ? version->value<QDBusVariant>().variant()
            : *version;
        if (value.metaType() != QMetaType::fromType<quint32>() || value.toUInt() > 4) {
            return false;
        }
        candidate.version = value.toUInt();
    }
    const auto status = values.constFind(QStringLiteral("Status"));
    if (status != values.cend()) {
        const QVariant value = status->metaType() == QMetaType::fromType<QDBusVariant>()
            ? status->value<QDBusVariant>().variant()
            : *status;
        if (value.metaType() != QMetaType::fromType<QString>()
            || (value.toString() != QStringLiteral("normal")
                && value.toString() != QStringLiteral("notice"))) {
            return false;
        }
        candidate.status = value.toString();
    }
    const auto direction = values.constFind(QStringLiteral("TextDirection"));
    if (direction != values.cend()) {
        const QVariant value = direction->metaType() == QMetaType::fromType<QDBusVariant>()
            ? direction->value<QDBusVariant>().variant()
            : *direction;
        if (value.metaType() != QMetaType::fromType<QString>()
            || (value.toString() != QStringLiteral("ltr")
                && value.toString() != QStringLiteral("rtl"))) {
            return false;
        }
        candidate.textDirection = value.toString();
    }
    *metadata = std::move(candidate);
    return true;
}

} // namespace

DbusMenuClient::DbusMenuClient(QDBusConnection connection, QString ownerUniqueName,
                               QDBusObjectPath objectPath, QUuid ownerWindowId,
                               int timeoutMilliseconds, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_ownerUniqueName(std::move(ownerUniqueName))
    , m_objectPath(std::move(objectPath))
    , m_ownerWindowId(ownerWindowId)
    , m_timeoutMilliseconds(timeoutMilliseconds)
{
    registerDbusMenuWireTypes();
}

DbusMenuClient::~DbusMenuClient()
{
    stop();
}

bool DbusMenuClient::start(QString *error)
{
    if (m_started) {
        setError(error, {});
        return true;
    }
    if (!m_connection.isConnected() || !m_ownerUniqueName.startsWith(u':')
        || m_objectPath.path().isEmpty() || m_ownerWindowId.isNull()
        || m_timeoutMilliseconds <= 0) {
        setError(error, QStringLiteral("invalid dbusmenu client configuration"));
        return false;
    }
    const bool layoutConnected = m_connection.connect(
        m_ownerUniqueName, m_objectPath.path(), QString::fromLatin1(kDbusMenuInterface),
        QStringLiteral("LayoutUpdated"), this, SLOT(layoutUpdated(uint,int)));
    const bool itemsConnected = m_connection.connect(
        m_ownerUniqueName, m_objectPath.path(), QString::fromLatin1(kDbusMenuInterface),
        QStringLiteral("ItemsPropertiesUpdated"), this,
        SLOT(itemsPropertiesUpdated(QindaQt::Shell::GlobalMenu::DbusMenu::PropertyEntryList,QindaQt::Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList)));
    const bool propertiesConnected = m_connection.connect(
        m_ownerUniqueName, m_objectPath.path(), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
    if (!layoutConnected || !itemsConnected || !propertiesConnected) {
        disconnectSignals();
        setError(error, QStringLiteral("failed to subscribe to dbusmenu signals"));
        return false;
    }
    m_ownerWatcher = new QDBusServiceWatcher(
        m_ownerUniqueName, m_connection, QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_ownerWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &owner) {
                if (owner == m_ownerUniqueName) {
                    handleOwnerLoss();
                }
            });
    m_started = true;
    ++m_generation;
    refreshLayout();
    requestMetadata();
    setError(error, {});
    return true;
}

void DbusMenuClient::disconnectSignals()
{
    (void)m_connection.disconnect(m_ownerUniqueName, m_objectPath.path(),
                                  QString::fromLatin1(kDbusMenuInterface),
                                  QStringLiteral("LayoutUpdated"), this,
                                  SLOT(layoutUpdated(uint,int)));
    (void)m_connection.disconnect(m_ownerUniqueName, m_objectPath.path(),
                                  QString::fromLatin1(kDbusMenuInterface),
                                  QStringLiteral("ItemsPropertiesUpdated"), this,
                                  SLOT(itemsPropertiesUpdated(QindaQt::Shell::GlobalMenu::DbusMenu::PropertyEntryList,QindaQt::Shell::GlobalMenu::DbusMenu::RemovedPropertyEntryList)));
    (void)m_connection.disconnect(m_ownerUniqueName, m_objectPath.path(),
                                  QString::fromLatin1(kPropertiesInterface),
                                  QStringLiteral("PropertiesChanged"), this,
                                  SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
}

void DbusMenuClient::retirePendingCalls()
{
    for (QDBusPendingCallWatcher *watcher : std::as_const(m_pendingCalls)) {
        watcher->disconnect(this);
        watcher->deleteLater();
    }
    m_pendingCalls.clear();
}

void DbusMenuClient::stop()
{
    if (!m_started && m_ownerWatcher == nullptr) {
        return;
    }
    m_started = false;
    ++m_generation;
    ++m_layoutSerial;
    disconnectSignals();
    retirePendingCalls();
    delete m_ownerWatcher;
    m_ownerWatcher = nullptr;
    m_layoutInFlight = false;
    m_layoutDirty = false;
    m_aboutToShowPending = 0;
    m_layoutValid = false;
    setLayoutCurrent(false);
    m_snapshot.reset();
    m_metadata = {};
    m_remoteRevision = 0;
}

bool DbusMenuClient::isStarted() const noexcept
{
    return m_started;
}

Exporter::MenuSnapshot DbusMenuClient::snapshot() const
{
    if (m_snapshot) {
        return *m_snapshot;
    }
    return Exporter::MenuSnapshot{.tree = {},
                                  .complete = false,
                                  .defectCode = QStringLiteral("dbusmenu-not-ready")};
}

ClientMetadata DbusMenuClient::metadata() const
{
    return m_metadata;
}

quint32 DbusMenuClient::remoteRevision() const noexcept
{
    return m_remoteRevision;
}

bool DbusMenuClient::isLayoutCurrent() const noexcept
{
    return m_layoutCurrent;
}

void DbusMenuClient::setLayoutCurrent(bool current)
{
    if (m_layoutCurrent != current) {
        m_layoutCurrent = current;
        Q_EMIT layoutCurrentChanged();
    }
}

void DbusMenuClient::refreshLayout()
{
    if (!m_started) {
        return;
    }
    m_layoutValid = false;
    setLayoutCurrent(false);
    if (m_layoutInFlight) {
        m_layoutDirty = true;
        return;
    }
    m_layoutInFlight = true;
    m_layoutDirty = false;
    const quint64 generation = m_generation;
    const quint64 serial = ++m_layoutSerial;
    QDBusMessage call = methodCall(m_ownerUniqueName, m_objectPath, kDbusMenuInterface, "GetLayout");
    call << qint32{0} << qint32{Protocol::kMaxDepth}
         << QStringList{QStringLiteral("label"), QStringLiteral("enabled"),
                        QStringLiteral("visible"), QStringLiteral("type"),
                        QStringLiteral("children-display"), QStringLiteral("toggle-type"),
                        QStringLiteral("toggle-state"), QStringLiteral("shortcut"),
                        QStringLiteral("icon-name"), QStringLiteral("icon-data")};
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, m_timeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, serial] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<quint32, LayoutItem> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_generation || serial != m_layoutSerial) {
                    return;
                }
                m_layoutInFlight = false;
                if (m_layoutDirty) {
                    refreshLayout();
                    return;
                }
                if (reply.isError()) {
                    if (!m_snapshot) {
                        Q_EMIT initialLayoutFailed(QStringLiteral("get-layout-failed"));
                    }
                    Q_EMIT rejected(QStringLiteral("get-layout-failed"));
                    return;
                }
                const quint32 revision = reply.argumentAt<0>();
                DecodeResult decoded = decodeLayout(m_ownerWindowId, revision, reply.argumentAt<1>());
                if (!decoded.accepted) {
                    if (!m_snapshot) {
                        Q_EMIT initialLayoutFailed(decoded.reasonCode);
                    }
                    Q_EMIT rejected(decoded.reasonCode);
                    return;
                }
                if (m_snapshot && revision < m_remoteRevision) {
                    Q_EMIT rejected(QStringLiteral("stale-layout-revision"));
                    return;
                }
                if (m_snapshot && revision == m_remoteRevision) {
                    if (m_snapshot->tree.items != decoded.snapshot.tree.items) {
                        Q_EMIT rejected(QStringLiteral("changed-equal-layout-revision"));
                    } else {
                        m_layoutValid = true;
                        setLayoutCurrent(m_aboutToShowPending == 0);
                    }
                    return;
                }
                m_remoteRevision = revision;
                m_snapshot = std::move(decoded.snapshot);
                m_layoutValid = true;
                Q_EMIT treeChanged();
                setLayoutCurrent(m_aboutToShowPending == 0);
            });
}

void DbusMenuClient::requestMetadata()
{
    if (!m_started) {
        return;
    }
    const quint64 generation = m_generation;
    QDBusMessage call = methodCall(m_ownerUniqueName, m_objectPath, kPropertiesInterface, "GetAll");
    call << QString::fromLatin1(kDbusMenuInterface);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, m_timeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_generation) {
                    return;
                }
                ClientMetadata metadata;
                if (reply.isError() || !validMetadataMap(reply.value(), &metadata)) {
                    Q_EMIT rejected(QStringLiteral("invalid-dbusmenu-properties"));
                    return;
                }
                if (metadata != m_metadata) {
                    m_metadata = std::move(metadata);
                    Q_EMIT metadataChanged();
                }
            });
}

void DbusMenuClient::requestGroupProperties(const QList<qint32> &itemIds)
{
    if (!m_started || itemIds.isEmpty() || itemIds.size() > Protocol::kMaxTotalItems) {
        Q_EMIT rejected(QStringLiteral("invalid-group-property-request"));
        return;
    }
    const quint64 generation = m_generation;
    QDBusMessage call = methodCall(m_ownerUniqueName, m_objectPath, kDbusMenuInterface,
                                   "GetGroupProperties");
    call << QVariant::fromValue(itemIds) << QStringList{};
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, m_timeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<PropertyEntryList> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_generation) {
                    return;
                }
                QString reason;
                if (reply.isError() || !validatePropertyUpdates(reply.value(), {}, &reason)) {
                    Q_EMIT rejected(reply.isError() ? QStringLiteral("get-group-properties-failed")
                                                    : reason);
                    return;
                }
                // Group properties carry no revision. They can validate the
                // endpoint, but publication waits for one complete revisioned
                // layout so an old property reply can never overwrite truth.
                refreshLayout();
            });
}

void DbusMenuClient::aboutToShow(qint32 itemId)
{
    if (!m_started || itemId < 0) {
        Q_EMIT rejected(QStringLiteral("invalid-about-to-show-request"));
        return;
    }
    // AGENT-CONTRACT: dbusmenu root id 0 is legal for AboutToShow, while
    // Event remains positive-id-only. Block current-action dispatch until the
    // exporter has finished preparing the root/submenu and any required read.
    ++m_aboutToShowPending;
    setLayoutCurrent(false);
    const quint64 generation = m_generation;
    QDBusMessage call = methodCall(m_ownerUniqueName, m_objectPath, kDbusMenuInterface,
                                   "AboutToShow");
    call << itemId;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, m_timeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<bool> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_generation) {
                    return;
                }
                --m_aboutToShowPending;
                if (reply.isError()) {
                    Q_EMIT rejected(QStringLiteral("about-to-show-failed"));
                } else if (reply.value()) {
                    refreshLayout();
                } else if (m_layoutValid && !m_layoutInFlight && !m_layoutDirty && m_snapshot) {
                    setLayoutCurrent(m_aboutToShowPending == 0);
                }
            });
}

void DbusMenuClient::sendEvent(qint32 itemId, const QString &eventId,
                               const QVariant &data, quint32 timestamp)
{
    if (!m_started || itemId <= 0 || eventId.isEmpty() || eventId.toUtf8().size() > 64) {
        Q_EMIT eventCompleted(itemId, false, QStringLiteral("invalid-event"));
        return;
    }
    const quint64 generation = m_generation;
    QDBusMessage call = methodCall(m_ownerUniqueName, m_objectPath, kDbusMenuInterface, "Event");
    const QVariant eventData = data.isValid() ? data : QVariant::fromValue(qint32{0});
    call << itemId << eventId << QVariant::fromValue(QDBusVariant(eventData)) << timestamp;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(call, m_timeoutMilliseconds), this);
    m_pendingCalls.append(watcher);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, itemId] {
                m_pendingCalls.removeAll(watcher);
                const QDBusPendingReply<> reply = *watcher;
                watcher->deleteLater();
                if (!m_started || generation != m_generation) {
                    return;
                }
                // AGENT-GUARD: completion is observation only. An error or
                // timeout is uncertain and must never replay Event, otherwise
                // one user intent could activate twice.
                Q_EMIT eventCompleted(itemId, !reply.isError(),
                                      reply.isError() ? QStringLiteral("event-uncertain")
                                                      : QString{});
            });
}

void DbusMenuClient::layoutUpdated(quint32 revision, qint32)
{
    if (!m_started || (m_snapshot && revision <= m_remoteRevision)) {
        return;
    }
    refreshLayout();
}

void DbusMenuClient::itemsPropertiesUpdated(PropertyEntryList updated,
                                            RemovedPropertyEntryList removed)
{
    QString reason;
    if (!m_started || !validatePropertyUpdates(updated, removed, &reason)) {
        if (m_started) {
            Q_EMIT rejected(reason);
        }
        return;
    }
    refreshLayout();
}

void DbusMenuClient::propertiesChanged(QString interfaceName, QVariantMap, QStringList)
{
    if (m_started && interfaceName == QString::fromLatin1(kDbusMenuInterface)) {
        requestMetadata();
    }
}

void DbusMenuClient::handleOwnerLoss()
{
    if (!m_started) {
        return;
    }
    stop();
    Q_EMIT unavailable();
}

} // namespace QindaQt::Shell::GlobalMenu::DbusMenu
