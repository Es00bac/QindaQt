// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QPointer>
#include <QTimer>

#include <optional>

namespace QindaQt::StatusNotifier
{
namespace
{

// Property decoders. A property whose top-level type does not match the wire
// shape is ignored (treated as absent); an entry with a hostile inner shape
// fails the whole descriptor closed so the registry can degrade truthfully
// while keeping the last-known-good item presented.
//
// AGENT-NOTE: QtDBus marshals a QVariantList of equal-length inner lists as a
// D-Bus struct array, so after a wire round trip every pixmap entry (and the
// ToolTip itself) demarshals as a QDBusArgument struct, not a QVariantList.
// Both decoders therefore accept both shapes; the struct form is what real
// StatusNotifier items produce.
[[nodiscard]] bool decodePixmapFields(const QDBusArgument *argument, Pixmap *out)
{
    // AGENT-GUARD: demarshalling overloads are const-qualified; a non-const
    // QDBusArgument silently selects the WRITING beginStructure/beginArray
    // (libdbus then aborts on "write from a read-only object").
    // currentType() must be checked first: a hostile entry whose inner shape
    // is not a struct (e.g. array of strings) crashes libdbus inside
    // beginStructure rather than failing cleanly.
    if (argument->currentType() != QDBusArgument::StructureType) {
        return false;
    }
    int width = 0;
    int height = 0;
    QByteArray argb;
    argument->beginStructure();
    *argument >> width >> height >> argb;
    argument->endStructure();
    if (width <= 0 || height <= 0) {
        return false;
    }
    out->width = quint32(width);
    out->height = quint32(height);
    out->argb = std::move(argb);
    return true;
}

[[nodiscard]] bool decodePixmapEntry(const QVariant &entry, Pixmap *out)
{
    if (entry.canConvert<QDBusArgument>()) {
        const QDBusArgument argument = entry.value<QDBusArgument>();
        return decodePixmapFields(&argument, out);
    }
    const QVariantList fields = entry.toList();
    if (fields.size() != 3) {
        return false;
    }
    bool widthOk = false;
    bool heightOk = false;
    const int width = fields.at(0).toInt(&widthOk);
    const int height = fields.at(1).toInt(&heightOk);
    if (!widthOk || !heightOk || width <= 0 || height <= 0) {
        return false;
    }
    out->width = quint32(width);
    out->height = quint32(height);
    out->argb = fields.at(2).toByteArray();
    return true;
}

[[nodiscard]] bool decodePixmapList(const QVariant &value, QList<Pixmap> *out)
{
    if (value.canConvert<QDBusArgument>()) {
        // Wire form a(iiay): demarshal the array element by element. A
        // hostile non-array argument must fail closed, not reach libdbus.
        const QDBusArgument array = value.value<QDBusArgument>();
        if (array.currentType() != QDBusArgument::ArrayType) {
            return false;
        }
        array.beginArray();
        while (!array.atEnd()) {
            Pixmap pixmap;
            if (!decodePixmapFields(&array, &pixmap)) {
                return false;
            }
            out->append(std::move(pixmap));
        }
        array.endArray();
        return true;
    }
    if (!value.canConvert<QVariantList>()) {
        // A type that is neither a wire argument nor a variant list (e.g. a
        // registered struct container in a process that also hosts the item
        // implementation) is not a shape real items produce; fail closed.
        return false;
    }
    const QVariantList entries = value.toList();
    for (const QVariant &entry : entries) {
        Pixmap pixmap;
        if (!decodePixmapEntry(entry, &pixmap)) {
            return false;
        }
        out->append(std::move(pixmap));
    }
    return true;
}

[[nodiscard]] std::optional<ItemCategory> decodeCategory(const QString &text)
{
    if (text == QStringLiteral("ApplicationStatus")) {
        return ItemCategory::Application;
    }
    if (text == QStringLiteral("Communications")) {
        return ItemCategory::Communications;
    }
    if (text == QStringLiteral("SystemServices")) {
        return ItemCategory::SystemServices;
    }
    if (text == QStringLiteral("Hardware")) {
        return ItemCategory::Hardware;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ItemStatus> decodeStatus(const QString &text)
{
    if (text == QStringLiteral("Passive")) {
        return ItemStatus::Passive;
    }
    if (text == QStringLiteral("Active")) {
        return ItemStatus::Active;
    }
    if (text == QStringLiteral("NeedsAttention")) {
        return ItemStatus::NeedsAttention;
    }
    return std::nullopt;
}

[[nodiscard]] ValidationOutcome decodeToolTip(const QVariant &value,
                                              ToolTipPayload *toolTip)
{
    if (value.canConvert<QDBusArgument>()) {
        // Wire form (sa(iiay)ss): demarshal the struct field by field.
        const QDBusArgument argument = value.value<QDBusArgument>();
        argument.beginStructure();
        QString iconName;
        QString title;
        QString description;
        argument >> iconName;
        // AGENT-NOTE: the a(iiay) field must NOT be read into a QVariant (or a
        // variant container): when the client process has a matching type
        // registered (in-process fakes, or a shell linking item-side
        // libraries such as KDE's DBusImageStruct), Qt 6.11's QVariant-field
        // demarshal consumes the array and leaves a mispositioned argument
        // that aborts in libdbus on first read. Iterating the array manually
        // with raw typed field reads is registry-independent, so the SAME
        // code serves production items and same-process fakes.
        argument.beginArray();
        while (!argument.atEnd()) {
            Pixmap pixmap;
            if (!decodePixmapFields(&argument, &pixmap)) {
                argument.endArray();
                return ValidationOutcome::failure(
                    ValidationError::InvalidToolTip,
                    QStringLiteral("tooltip-pixmap-decode-failed"));
            }
            toolTip->pixmaps.append(std::move(pixmap));
        }
        argument.endArray();
        argument >> title >> description;
        argument.endStructure();
        toolTip->iconName = iconName;
        toolTip->title = title;
        toolTip->description = description;
        return ValidationOutcome::success();
    }
    if (!value.canConvert<QVariantList>()) {
        return ValidationOutcome::success(); // Wrong type: tooltip ignored.
    }
    const QVariantList fields = value.toList();
    if (fields.size() != 4) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("tooltip-decode-failed"));
    }
    toolTip->iconName = fields.at(0).toString();
    if (!decodePixmapList(fields.at(1), &toolTip->pixmaps)) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("tooltip-pixmap-decode-failed"));
    }
    toolTip->title = fields.at(2).toString();
    toolTip->description = fields.at(3).toString();
    return ValidationOutcome::success();
}

// Decodes the full GetAll property map into the foundation descriptor plus
// the recorded wire details, then runs the single admission gate.
[[nodiscard]] ValidationOutcome decodeDescriptor(const QVariantMap &properties,
                                                 ItemDescriptor *descriptor,
                                                 ItemWireDetails *wire)
{
    ValidationOutcome outcome = ValidationOutcome::success();
    const auto fail = [&outcome](ValidationError error, const QString &reason) {
        if (outcome.accepted) {
            outcome = ValidationOutcome::failure(error, reason);
        }
    };

    const auto category =
        decodeCategory(properties.value(QStringLiteral("Category")).toString());
    if (category.has_value()) {
        descriptor->category = *category;
    } else {
        fail(ValidationError::InvalidCategory, QStringLiteral("category-decode-failed"));
    }
    const auto status = decodeStatus(properties.value(QStringLiteral("Status")).toString());
    if (status.has_value()) {
        descriptor->status = *status;
    } else {
        fail(ValidationError::InvalidStatus, QStringLiteral("status-decode-failed"));
    }

    descriptor->identity = properties.value(QStringLiteral("Id")).toString();
    descriptor->title = properties.value(QStringLiteral("Title")).toString();
    descriptor->icon.iconName = properties.value(QStringLiteral("IconName")).toString();
    if (!decodePixmapList(properties.value(QStringLiteral("IconPixmap")),
                          &descriptor->icon.pixmaps)) {
        fail(ValidationError::InvalidIcon, QStringLiteral("iconpixmap-decode-failed"));
    }
    descriptor->icon.attentionIconName =
        properties.value(QStringLiteral("AttentionIconName")).toString();
    if (!decodePixmapList(properties.value(QStringLiteral("AttentionPixmap")),
                          &descriptor->icon.attentionPixmaps)) {
        fail(ValidationError::InvalidIcon, QStringLiteral("attentionpixmap-decode-failed"));
    }
    descriptor->icon.attentionMovieName =
        properties.value(QStringLiteral("AttentionMovieName")).toString();

    const ValidationOutcome toolTip =
        decodeToolTip(properties.value(QStringLiteral("ToolTip")), &descriptor->toolTip);
    if (!toolTip.accepted) {
        fail(toolTip.error, toolTip.reasonCode);
    }

    // Recorded-not-rendered wire details stay lenient: a hostile overlay name
    // or menu path cannot reach presentation, so it is dropped, not fatal.
    const QString overlay = properties.value(QStringLiteral("OverlayIconName")).toString();
    wire->overlayIconName =
        isAcceptableOptionalText(overlay, kMaxIconNameUtf8Bytes) ? overlay : QString();
    wire->itemIsMenu = properties.value(QStringLiteral("ItemIsMenu")).toBool();
    const QVariant menuVariant = properties.value(QStringLiteral("Menu"));
    const QString menu = menuVariant.canConvert<QDBusObjectPath>()
        ? menuVariant.value<QDBusObjectPath>().path()
        : menuVariant.toString();
    wire->menuObjectPath = isValidObjectPath(menu) ? menu : QString();
    wire->windowId = properties.value(QStringLiteral("WindowId")).toUInt();

    if (outcome.accepted) {
        outcome = validateItemDescriptor(*descriptor);
    }
    return outcome;
}

} // namespace

StatusNotifierItemClient::StatusNotifierItemClient(QDBusConnection connection,
                                                   const OwnerKey &key,
                                                   GenerationFence generationFence,
                                                   int fetchTimeoutMs,
                                                   QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_key(key)
    , m_generationFence(std::move(generationFence))
    , m_fetchTimeoutMs(fetchTimeoutMs)
{
    const QString service = m_key.uniqueName;
    const QString path = m_key.objectPath;
    const QString interface = QString::fromLatin1(kItemInterfaceName);
    m_connection.connect(service, path, interface, QStringLiteral("NewTitle"),
                         this, SLOT(handleNewTitle()));
    m_connection.connect(service, path, interface, QStringLiteral("NewIcon"),
                         this, SLOT(handleNewIcon()));
    m_connection.connect(service, path, interface, QStringLiteral("NewAttentionIcon"),
                         this, SLOT(handleNewAttentionIcon()));
    m_connection.connect(service, path, interface, QStringLiteral("NewOverlayIcon"),
                         this, SLOT(handleNewOverlayIcon()));
    m_connection.connect(service, path, interface, QStringLiteral("NewToolTip"),
                         this, SLOT(handleNewToolTip()));
    m_connection.connect(service, path, interface, QStringLiteral("NewStatus"),
                         this, SLOT(handleNewStatus(QString)));
    m_connection.connect(service, path, interface, QStringLiteral("NewIconThemePath"),
                         this, SLOT(handleNewIconThemePath(QString)));
}

StatusNotifierItemClient::~StatusNotifierItemClient() = default;

OwnerKey StatusNotifierItemClient::key() const
{
    return m_key;
}

void StatusNotifierItemClient::fetchDescriptor()
{
    if (m_fetchInFlight || !fenceOpen()) {
        return;
    }
    m_fetchInFlight = true;

    // AGENT-NOTE: The GetAll call goes out with an EMPTY interface field.
    // "org.freedesktop.D-Bus.Properties" is not a spec-valid interface name
    // (hyphens), so QDBusConnection::asyncCall rejects it client-side and
    // QDBusInterface silently drops it; with an empty interface the daemon
    // and Qt's dispatcher match by member name and reach the callee's
    // Properties adaptor, which is how every real StatusNotifier item serves
    // the call. Qt's server side also cannot serve Get/GetAll for plain
    // ExportAllProperties objects (UnknownInterface) — real items own a
    // QDBusAbstractAdaptor, so requiring one is correct, not hostile.
    auto request = QDBusMessage::createMethodCall(
        m_key.uniqueName,
        m_key.objectPath,
        QString(),
        QStringLiteral("GetAll"));
    request << QVariant(QString::fromLatin1(kItemInterfaceName));
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(request),
                                                this);
    QTimer::singleShot(m_fetchTimeoutMs, this, [this, watcher]() {
        if (!m_fetchInFlight) {
            return; // The reply path already reported.
        }
        // QPointer guard: the finished lambda may already have deleteLater()d
        // (and event delivery freed) the watcher before this timer fires.
        if (watcher) {
            watcher->deleteLater();
        }
        finishFetch(ItemDescriptorFetch{});
    });
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                if (!m_fetchInFlight) {
                    return; // The timeout path already reported.
                }
                const QDBusMessage reply = call->reply();
                if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                    finishFetch(ItemDescriptorFetch{});
                    return;
                }
                ItemDescriptorFetch result;
                result.replyReceived = true;
                result.key = m_key;
                result.generation = m_key.generation;
                // The GetAll payload arrives as a QDBusArgument (a{sv}), not
                // a QVariantMap; demarshal before decoding.
                QVariantMap properties;
                const QVariant payload = reply.arguments().constFirst();
                if (payload.canConvert<QDBusArgument>()) {
                    payload.value<QDBusArgument>() >> properties;
                } else {
                    properties = payload.toMap();
                }
                result.validation =
                    decodeDescriptor(properties, &result.descriptor, &result.wire);
                finishFetch(std::move(result));
            });
}

void StatusNotifierItemClient::activate(int x, int y)
{
    sendIntent(QStringLiteral("Activate"),
               {QVariant(x), QVariant(quint32(y))});
}

void StatusNotifierItemClient::secondaryActivate(int x, int y)
{
    sendIntent(QStringLiteral("SecondaryActivate"),
               {QVariant(x), QVariant(quint32(y))});
}

void StatusNotifierItemClient::contextMenu(int x, int y)
{
    sendIntent(QStringLiteral("ContextMenu"),
               {QVariant(x), QVariant(quint32(y))});
}

bool StatusNotifierItemClient::scroll(int delta, const QString &orientation)
{
    if (orientation != QStringLiteral("horizontal")
        && orientation != QStringLiteral("vertical")) {
        return false;
    }
    sendIntent(QStringLiteral("Scroll"), {QVariant(delta), QVariant(orientation)});
    return true;
}

void StatusNotifierItemClient::handleNewTitle()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewIcon()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewAttentionIcon()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewOverlayIcon()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewToolTip()
{
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewStatus(const QString &status)
{
    Q_UNUSED(status)
    scheduleRefetch();
}

void StatusNotifierItemClient::handleNewIconThemePath(const QString &path)
{
    Q_UNUSED(path)
    scheduleRefetch();
}

void StatusNotifierItemClient::scheduleRefetch()
{
    fetchDescriptor();
}

void StatusNotifierItemClient::sendIntent(const QString &member,
                                          const QList<QVariant> &arguments)
{
    auto message = QDBusMessage::createMethodCall(m_key.uniqueName,
                                                  m_key.objectPath,
                                                  QString::fromLatin1(kItemInterfaceName),
                                                  member);
    message.setArguments(arguments);
    const bool sent = m_connection.send(message);
    Q_UNUSED(sent)
}

bool StatusNotifierItemClient::fenceOpen() const
{
    return m_key.generation != 0 && m_generationFence != nullptr
        && m_generationFence(m_key.generation);
}

void StatusNotifierItemClient::finishFetch(ItemDescriptorFetch result)
{
    if (!m_fetchInFlight) {
        return;
    }
    m_fetchInFlight = false;
    if (!fenceOpen()) {
        // AGENT-GUARD: A reply that raced owner loss, removal, or a watcher
        // rebaseline is dropped here, not emitted; the registry fences again
        // on registration, so a removed item can never be resurrected.
        return;
    }
    result.key = m_key;
    result.generation = m_key.generation;
    emit descriptorFetched(std::move(result));
}

} // namespace QindaQt::StatusNotifier
