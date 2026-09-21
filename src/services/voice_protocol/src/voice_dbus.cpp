// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/voice_protocol/voice_dbus.h>

#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtDBus/QDBusArgument>

#include <array>
#include <limits>

namespace QindaQt::Services::Voice {
namespace {

// AGENT-GUARD: these key names are the wire contract with every provider,
// Gabbee's src/gabbee/qindaqt_voice.py included. Renaming one is a schema
// version bump on both sides, never an in-place edit.
constexpr char kKeySchemaVersion[] = "schemaVersion";
constexpr char kKeyRevision[] = "revision";
constexpr char kKeyState[] = "state";
constexpr char kKeyMode[] = "mode";
constexpr char kKeyEnabled[] = "enabled";
constexpr char kKeyCapabilities[] = "capabilities";
constexpr char kKeyLastRoute[] = "lastRoute";
constexpr char kKeyProviderId[] = "providerId";
constexpr char kKeyProviderLabel[] = "providerLabel";
constexpr char kKeyLanguageCode[] = "languageCode";
constexpr char kKeyMicrophoneLabel[] = "microphoneLabel";
constexpr char kKeyDictationShortcut[] = "dictationShortcut";
constexpr char kKeyCommandShortcut[] = "commandShortcut";
constexpr char kKeyPartialText[] = "partialText";
constexpr char kKeyLastText[] = "lastText";
constexpr char kKeyReasonCode[] = "reasonCode";
// AGENT-CONTRACT: the provider list is three parallel scalars, not a nested
// container. a{sv} values of type aa{sv} marshal differently across bindings
// (PyQt produces av-of-variant, Qt produces aa{sv}), and a provider list is
// not worth a decoder that has to accept both. Parallel as/as/u arrays have
// exactly one representation everywhere.
constexpr char kKeyProviderIds[] = "providerIds";
constexpr char kKeyProviderLabels[] = "providerLabels";
constexpr char kKeyProviderAvailableMask[] = "providerAvailableMask";

constexpr char kKeyKind[] = "kind";
constexpr char kKeyStatus[] = "status";
constexpr char kKeyRequestId[] = "requestId";
constexpr char kKeyInitiatingRevision[] = "initiatingRevision";
constexpr char kKeyObservedRevision[] = "observedRevision";

constexpr std::array kSnapshotKeys{
    kKeySchemaVersion,    kKeyRevision,       kKeyState,
    kKeyMode,             kKeyEnabled,        kKeyCapabilities,
    kKeyLastRoute,        kKeyProviderId,     kKeyProviderLabel,
    kKeyLanguageCode,     kKeyMicrophoneLabel, kKeyDictationShortcut,
    kKeyCommandShortcut,  kKeyPartialText,    kKeyLastText,
    kKeyReasonCode,       kKeyProviderIds,    kKeyProviderLabels,
    kKeyProviderAvailableMask,
};

constexpr std::array kResultKeys{
    kKeyKind, kKeyStatus, kKeyRequestId, kKeyInitiatingRevision,
    kKeyObservedRevision, kKeyReasonCode,
};

template <std::size_t N>
bool keysAreExactly(const QVariantMap &payload, const std::array<const char *, N> &expected)
{
    if (payload.size() != static_cast<qsizetype>(expected.size())) {
        return false;
    }
    for (const char *key : expected) {
        if (!payload.contains(QString::fromLatin1(key))) {
            return false;
        }
    }
    return true;
}

// A provider may legitimately send 32-bit or 64-bit integers for a numeric
// field depending on its bindings, so the check is on the value's category and
// range rather than its exact metatype. A negative or oversized value fails.
bool readUInt32(const QVariant &value, quint32 *out)
{
    bool ok = false;
    const qulonglong parsed = value.toULongLong(&ok);
    if (!ok || parsed > std::numeric_limits<quint32>::max()) {
        return false;
    }
    if (value.typeId() == QMetaType::QString || value.typeId() == QMetaType::Bool) {
        return false;
    }
    *out = static_cast<quint32>(parsed);
    return true;
}

bool readUInt64(const QVariant &value, quint64 *out)
{
    bool ok = false;
    const qulonglong parsed = value.toULongLong(&ok);
    if (!ok || value.typeId() == QMetaType::QString
        || value.typeId() == QMetaType::Bool) {
        return false;
    }
    *out = parsed;
    return true;
}

bool readBool(const QVariant &value, bool *out)
{
    if (value.typeId() != QMetaType::Bool) {
        return false;
    }
    *out = value.toBool();
    return true;
}

bool readString(const QVariant &value, QString *out)
{
    if (value.typeId() != QMetaType::QString) {
        return false;
    }
    *out = value.toString();
    return true;
}

// A list of strings that arrived as a list of variants. Shared by the
// in-process QVariantList shape and the `av` shape off the wire so both are
// bounded and type-checked by exactly the same rule.
bool readStringsFromVariants(const QVariantList &raw, QStringList *out)
{
    if (raw.size() > kMaxProviders) {
        return false;
    }
    QStringList decoded;
    decoded.reserve(raw.size());
    for (const QVariant &entry : raw) {
        // Strictness is recovered here: a variant that does not hold a string
        // is a provider defect, not something to coerce.
        if (entry.typeId() != QMetaType::QString) {
            return false;
        }
        decoded.append(entry.toString());
    }
    *out = decoded;
    return true;
}

bool readStringList(const QVariant &value, QStringList *out)
{
    if (value.typeId() == QMetaType::QStringList) {
        const QStringList direct = value.toStringList();
        if (direct.size() > kMaxProviders) {
            return false;
        }
        *out = direct;
        return true;
    }
    // A binding that hands over a list of variants -- PyQt6 turns a Python
    // list into one -- is describing the same value as `as`; read it alike.
    if (value.typeId() == QMetaType::QVariantList) {
        return readStringsFromVariants(value.toList(), out);
    }
    if (!value.canConvert<QDBusArgument>()) {
        return false;
    }
    const QDBusArgument argument = value.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::ArrayType) {
        return false;
    }
    // AGENT-GUARD: dispatch on the element signature and let Qt walk the
    // array. A hand-written `while (!argument.atEnd())` looks equivalent and
    // is not: QDBusArgument does not advance when an element is not the type
    // being extracted, so the loop never ends and appends until the kernel
    // kills the process. The provider is replaceable and outside our control,
    // so a mistyped array must cost a refusal, never the shell it runs in.
    const QString signature = argument.currentSignature();
    // Bindings disagree on how a list of strings nests inside a{sv}: Qt sends
    // `as`, while PyQt6 turns a Python list into a QVariantList and sends
    // `av`. Both are honest encodings of the same value, so both are read.
    if (signature == QLatin1String("as")) {
        const QStringList decoded = qdbus_cast<QStringList>(argument);
        if (decoded.size() > kMaxProviders) {
            return false;
        }
        *out = decoded;
        return true;
    }
    if (signature == QLatin1String("av")) {
        return readStringsFromVariants(qdbus_cast<QVariantList>(argument), out);
    }
    return false;
}

bool readProviderList(const QVariant &idsValue, const QVariant &labelsValue,
                      const QVariant &maskValue, QList<ProviderDescriptor> *out)
{
    QStringList ids;
    QStringList labels;
    quint32 mask = 0;
    if (!readStringList(idsValue, &ids) || !readStringList(labelsValue, &labels)
        || !readUInt32(maskValue, &mask)) {
        return false;
    }
    if (ids.size() != labels.size() || ids.size() > kMaxProviders) {
        return false;
    }
    // A bit set past the end of the list describes a provider that was not
    // sent; that is a provider defect, not something to silently ignore.
    const quint32 usableBits =
        ids.size() >= 32 ? ~0u : ((1u << static_cast<quint32>(ids.size())) - 1u);
    if ((mask & ~usableBits) != 0) {
        return false;
    }
    out->clear();
    out->reserve(ids.size());
    for (qsizetype index = 0; index < ids.size(); ++index) {
        out->append(ProviderDescriptor{
            .id = ids.at(index),
            .label = labels.at(index),
            .available = (mask & (1u << static_cast<quint32>(index))) != 0,
        });
    }
    return true;
}

QStringList providerIds(const QList<ProviderDescriptor> &providers)
{
    QStringList ids;
    ids.reserve(providers.size());
    for (const ProviderDescriptor &provider : providers) {
        ids.append(provider.id);
    }
    return ids;
}

QStringList providerLabels(const QList<ProviderDescriptor> &providers)
{
    QStringList labels;
    labels.reserve(providers.size());
    for (const ProviderDescriptor &provider : providers) {
        labels.append(provider.label);
    }
    return labels;
}

quint32 providerAvailableMask(const QList<ProviderDescriptor> &providers)
{
    quint32 mask = 0;
    for (qsizetype index = 0; index < providers.size() && index < 32; ++index) {
        if (providers.at(index).available) {
            mask |= 1u << static_cast<quint32>(index);
        }
    }
    return mask;
}

} // namespace

QVariantMap encodeSnapshot(const Snapshot &snapshot)
{
    return {
        {QString::fromLatin1(kKeySchemaVersion), snapshot.schemaVersion},
        {QString::fromLatin1(kKeyRevision), snapshot.revision},
        {QString::fromLatin1(kKeyState), static_cast<quint32>(snapshot.state)},
        {QString::fromLatin1(kKeyMode), static_cast<quint32>(snapshot.mode)},
        {QString::fromLatin1(kKeyEnabled), snapshot.enabled},
        {QString::fromLatin1(kKeyCapabilities), snapshot.capabilities},
        {QString::fromLatin1(kKeyLastRoute), static_cast<quint32>(snapshot.lastRoute)},
        {QString::fromLatin1(kKeyProviderId), snapshot.providerId},
        {QString::fromLatin1(kKeyProviderLabel), snapshot.providerLabel},
        {QString::fromLatin1(kKeyLanguageCode), snapshot.languageCode},
        {QString::fromLatin1(kKeyMicrophoneLabel), snapshot.microphoneLabel},
        {QString::fromLatin1(kKeyDictationShortcut), snapshot.dictationShortcut},
        {QString::fromLatin1(kKeyCommandShortcut), snapshot.commandShortcut},
        {QString::fromLatin1(kKeyPartialText), snapshot.partialText},
        {QString::fromLatin1(kKeyLastText), snapshot.lastText},
        {QString::fromLatin1(kKeyReasonCode), snapshot.reasonCode},
        {QString::fromLatin1(kKeyProviderIds), providerIds(snapshot.providers)},
        {QString::fromLatin1(kKeyProviderLabels), providerLabels(snapshot.providers)},
        {QString::fromLatin1(kKeyProviderAvailableMask),
         providerAvailableMask(snapshot.providers)},
    };
}

Snapshot decodeSnapshot(const QVariantMap &payload)
{
    Snapshot snapshot;
    snapshot.wireValid = false;
    if (!keysAreExactly(payload, kSnapshotKeys)) {
        return snapshot;
    }
    quint32 state = 0;
    quint32 mode = 0;
    quint32 route = 0;
    const auto at = [&payload](const char *key) {
        return payload.value(QString::fromLatin1(key));
    };
    if (!readUInt32(at(kKeySchemaVersion), &snapshot.schemaVersion)
        || !readUInt64(at(kKeyRevision), &snapshot.revision)
        || !readUInt32(at(kKeyState), &state) || !readUInt32(at(kKeyMode), &mode)
        || !readBool(at(kKeyEnabled), &snapshot.enabled)
        || !readUInt32(at(kKeyCapabilities), &snapshot.capabilities)
        || !readUInt32(at(kKeyLastRoute), &route)
        || !readString(at(kKeyProviderId), &snapshot.providerId)
        || !readString(at(kKeyProviderLabel), &snapshot.providerLabel)
        || !readString(at(kKeyLanguageCode), &snapshot.languageCode)
        || !readString(at(kKeyMicrophoneLabel), &snapshot.microphoneLabel)
        || !readString(at(kKeyDictationShortcut), &snapshot.dictationShortcut)
        || !readString(at(kKeyCommandShortcut), &snapshot.commandShortcut)
        || !readString(at(kKeyPartialText), &snapshot.partialText)
        || !readString(at(kKeyLastText), &snapshot.lastText)
        || !readString(at(kKeyReasonCode), &snapshot.reasonCode)
        || !readProviderList(at(kKeyProviderIds), at(kKeyProviderLabels),
                             at(kKeyProviderAvailableMask), &snapshot.providers)) {
        Snapshot invalid;
        invalid.wireValid = false;
        return invalid;
    }
    // Out-of-range enum values are left to validateSnapshot(), which names the
    // exact reason; the payload itself decoded correctly.
    snapshot.state = static_cast<SessionState>(state);
    snapshot.mode = static_cast<CaptureMode>(mode);
    snapshot.lastRoute = static_cast<DeliveryRoute>(route);
    snapshot.wireValid = true;
    return snapshot;
}

QVariantMap encodeOperationResult(const OperationResult &result)
{
    return {
        {QString::fromLatin1(kKeyKind), static_cast<quint32>(result.kind)},
        {QString::fromLatin1(kKeyStatus), static_cast<quint32>(result.status)},
        {QString::fromLatin1(kKeyRequestId), result.requestId},
        {QString::fromLatin1(kKeyInitiatingRevision), result.initiatingRevision},
        {QString::fromLatin1(kKeyObservedRevision), result.observedRevision},
        {QString::fromLatin1(kKeyReasonCode), result.reasonCode},
    };
}

OperationResult decodeOperationResult(const QVariantMap &payload)
{
    OperationResult result;
    result.wireValid = false;
    if (!keysAreExactly(payload, kResultKeys)) {
        return result;
    }
    const auto at = [&payload](const char *key) {
        return payload.value(QString::fromLatin1(key));
    };
    quint32 kind = 0;
    quint32 status = 0;
    if (!readUInt32(at(kKeyKind), &kind) || !readUInt32(at(kKeyStatus), &status)
        || !readUInt64(at(kKeyRequestId), &result.requestId)
        || !readUInt64(at(kKeyInitiatingRevision), &result.initiatingRevision)
        || !readUInt64(at(kKeyObservedRevision), &result.observedRevision)
        || !readString(at(kKeyReasonCode), &result.reasonCode)) {
        OperationResult invalid;
        invalid.wireValid = false;
        return invalid;
    }
    result.kind = static_cast<OperationKind>(kind);
    result.status = static_cast<OperationStatus>(status);
    result.wireValid = true;
    return result;
}

QString methodNameForKind(const OperationKind kind)
{
    switch (kind) {
    case OperationKind::StartDictation: return QStringLiteral("StartDictation");
    case OperationKind::StartCommand:   return QStringLiteral("StartCommand");
    case OperationKind::Finish:         return QStringLiteral("Finish");
    case OperationKind::Cancel:         return QStringLiteral("Cancel");
    case OperationKind::Retry:          return QStringLiteral("Retry");
    case OperationKind::Undo:           return QStringLiteral("Undo");
    case OperationKind::CopyLast:       return QStringLiteral("CopyLast");
    case OperationKind::SetProvider:    return QStringLiteral("SetProvider");
    case OperationKind::SetEnabled:     return QStringLiteral("SetEnabled");
    }
    return {};
}

QVariantList argumentsForRequest(const OperationRequest &request)
{
    QVariantList arguments{request.requestId, request.expectedRevision};
    if (request.kind == OperationKind::SetProvider) {
        arguments.append(request.providerId);
    } else if (request.kind == OperationKind::SetEnabled) {
        arguments.append(request.enable);
    }
    return arguments;
}

} // namespace QindaQt::Services::Voice
