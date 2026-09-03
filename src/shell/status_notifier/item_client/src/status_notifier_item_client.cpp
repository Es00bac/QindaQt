// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QMetaType>
#include <QPointer>
#include <QTimer>

#include <optional>

namespace QindaQt::StatusNotifier
{
namespace
{

// Property decoders. Presentation-bearing recognized properties with a wrong
// top-level or inner wire type fail the descriptor closed so the registry can
// degrade truthfully while keeping the last-known-good item presented. The
// explicitly recorded-only details are safe-dropped on wrong types because
// this module neither admits them to the registry nor renders them.
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
    if (fields.size() != 3
        || fields.at(0).metaType() != QMetaType::fromType<int>()
        || fields.at(1).metaType() != QMetaType::fromType<int>()
        || fields.at(2).metaType() != QMetaType::fromType<QByteArray>()) {
        return false;
    }
    const int width = fields.at(0).toInt();
    const int height = fields.at(1).toInt();
    if (width <= 0 || height <= 0) {
        return false;
    }
    out->width = quint32(width);
    out->height = quint32(height);
    out->argb = fields.at(2).toByteArray();
    return true;
}

[[nodiscard]] bool decodePixmapList(const QVariant &value, QList<Pixmap> *out)
{
    if (!value.isValid()) {
        return true; // Missing optional property decodes to an empty list.
    }
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
    if (!value.isValid()) {
        return ValidationOutcome::success();
    }
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
    if (value.metaType() != QMetaType::fromType<QVariantList>()) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("tooltip-type-invalid"));
    }
    const QVariantList fields = value.toList();
    if (fields.size() != 4) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("tooltip-decode-failed"));
    }
    if (fields.at(0).metaType() != QMetaType::fromType<QString>()
        || fields.at(2).metaType() != QMetaType::fromType<QString>()
        || fields.at(3).metaType() != QMetaType::fromType<QString>()) {
        return ValidationOutcome::failure(ValidationError::InvalidToolTip,
                                          QStringLiteral("tooltip-field-type-invalid"));
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

    const auto readString = [&properties, &fail](const QString &name,
                                                  QString *destination,
                                                  ValidationError error,
                                                  bool required) {
        const auto value = properties.constFind(name);
        if (value == properties.cend()) {
            if (required) {
                fail(error, name.toLower() + QStringLiteral("-missing"));
            }
            return;
        }
        if (value->metaType() != QMetaType::fromType<QString>()) {
            fail(error, name.toLower() + QStringLiteral("-type-invalid"));
            return;
        }
        *destination = value->toString();
    };

    QString categoryText;
    readString(QStringLiteral("Category"), &categoryText,
               ValidationError::InvalidCategory, true);
    const auto category = decodeCategory(categoryText);
    if (category.has_value()) {
        descriptor->category = *category;
    } else {
        fail(ValidationError::InvalidCategory, QStringLiteral("category-decode-failed"));
    }
    QString statusText;
    readString(QStringLiteral("Status"), &statusText,
               ValidationError::InvalidStatus, true);
    const auto status = decodeStatus(statusText);
    if (status.has_value()) {
        descriptor->status = *status;
    } else {
        fail(ValidationError::InvalidStatus, QStringLiteral("status-decode-failed"));
    }

    readString(QStringLiteral("Id"), &descriptor->identity,
               ValidationError::InvalidIdentity, true);
    readString(QStringLiteral("Title"), &descriptor->title,
               ValidationError::InvalidTitle, false);
    readString(QStringLiteral("IconName"), &descriptor->icon.iconName,
               ValidationError::InvalidIcon, false);
    if (!decodePixmapList(properties.value(QStringLiteral("IconPixmap")),
                          &descriptor->icon.pixmaps)) {
        fail(ValidationError::InvalidIcon, QStringLiteral("iconpixmap-decode-failed"));
    }
    readString(QStringLiteral("AttentionIconName"),
               &descriptor->icon.attentionIconName,
               ValidationError::InvalidIcon, false);
    if (!decodePixmapList(properties.value(QStringLiteral("AttentionPixmap")),
                          &descriptor->icon.attentionPixmaps)) {
        fail(ValidationError::InvalidIcon, QStringLiteral("attentionpixmap-decode-failed"));
    }
    readString(QStringLiteral("AttentionMovieName"),
               &descriptor->icon.attentionMovieName,
               ValidationError::InvalidIcon, false);

    const ValidationOutcome toolTip =
        decodeToolTip(properties.value(QStringLiteral("ToolTip")), &descriptor->toolTip);
    if (!toolTip.accepted) {
        fail(toolTip.error, toolTip.reasonCode);
    }

    // Recorded-not-rendered wire details stay lenient: a hostile overlay name
    // or menu path cannot reach presentation, so it is dropped, not fatal.
    QString overlay;
    const QVariant overlayValue = properties.value(QStringLiteral("OverlayIconName"));
    if (overlayValue.metaType() == QMetaType::fromType<QString>()) {
        overlay = overlayValue.toString();
    }
    wire->overlayIconName =
        isAcceptableOptionalText(overlay, kMaxIconNameUtf8Bytes) ? overlay : QString();
    const QVariant itemIsMenu = properties.value(QStringLiteral("ItemIsMenu"));
    wire->itemIsMenu = itemIsMenu.metaType() == QMetaType::fromType<bool>()
        && itemIsMenu.toBool();
    const QVariant menuVariant = properties.value(QStringLiteral("Menu"));
    const QString menu = menuVariant.metaType() == QMetaType::fromType<QDBusObjectPath>()
        ? menuVariant.value<QDBusObjectPath>().path()
        : QString();
    wire->menuObjectPath = isValidObjectPath(menu) ? menu : QString();
    const QVariant windowId = properties.value(QStringLiteral("WindowId"));
    if (windowId.metaType() == QMetaType::fromType<quint32>()) {
        wire->windowId = windowId.toUInt();
    }

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
    QPointer<QDBusPendingCallWatcher> watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(request), this);
    QTimer::singleShot(m_fetchTimeoutMs, this, [this, watcher]() {
        if (!m_fetchInFlight) {
            return; // The reply path already reported.
        }
        // QPointer guard: the finished lambda may already have deleteLater()d
        // (and event delivery freed) the watcher before this timer fires.
        if (watcher) {
            watcher->deleteLater();
        }
        ItemDescriptorFetch result;
        result.status = ItemDescriptorFetchStatus::TimedOut;
        finishFetch(std::move(result));
    });
    connect(watcher.data(), &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                if (!m_fetchInFlight) {
                    return; // The timeout path already reported.
                }
                const QDBusMessage reply = call->reply();
                if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                    ItemDescriptorFetch result;
                    result.status = ItemDescriptorFetchStatus::TransportError;
                    finishFetch(std::move(result));
                    return;
                }
                ItemDescriptorFetch result;
                result.status = ItemDescriptorFetchStatus::ReplyReceived;
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
    sendIntent(QStringLiteral("Activate"), {QVariant(x), QVariant(y)});
}

void StatusNotifierItemClient::secondaryActivate(int x, int y)
{
    sendIntent(QStringLiteral("SecondaryActivate"), {QVariant(x), QVariant(y)});
}

void StatusNotifierItemClient::contextMenu(int x, int y)
{
    sendIntent(QStringLiteral("ContextMenu"), {QVariant(x), QVariant(y)});
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
