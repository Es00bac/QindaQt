// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_dbus.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtDBus/QDBusMetaType>

namespace QindaQt::Display
{
namespace
{

bool hasSignature(const QDBusArgument &argument, const char *expected)
{
    // AGENT-GUARD: QDBusArgument does not expose a public busless conversion
    // from its write-only marshaller form to its read-only demarshaller form.
    // Reject non-inbound arguments before extraction; otherwise Qt warns and
    // the zero-initialized temporary could look like a semantic decode.
    return argument.currentType() == QDBusArgument::StructureType
        && argument.currentSignature().toLatin1() == expected;
}

template<typename T>
void writeArray(QDBusArgument &argument, const QList<T> &values)
{
    argument.beginArray(QMetaType::fromType<T>());
    for (const T &value : values) {
        argument << value;
    }
    argument.endArray();
}

template<typename T>
void readBoundedArray(const QDBusArgument &argument, QList<T> &values,
                      const qsizetype maximum, bool &wireValid)
{
    values.clear();
    argument.beginArray();
    while (!argument.atEnd()) {
        T value;
        argument >> value;
        if (values.size() < maximum) {
            values.push_back(std::move(value));
        } else {
            wireValid = false;
        }
    }
    argument.endArray();
}

} // namespace

void registerDBusTypes()
{
    qRegisterMetaType<Mode>();
    qRegisterMetaType<Output>();
    qRegisterMetaType<CandidateOutput>();
    qRegisterMetaType<Candidate>();
    qRegisterMetaType<TransactionSummary>();
    qRegisterMetaType<Snapshot>();
    qRegisterMetaType<OperationResult>();
    qDBusRegisterMetaType<Mode>();
    qDBusRegisterMetaType<Output>();
    qDBusRegisterMetaType<CandidateOutput>();
    qDBusRegisterMetaType<Candidate>();
    qDBusRegisterMetaType<TransactionSummary>();
    qDBusRegisterMetaType<Snapshot>();
    qDBusRegisterMetaType<OperationResult>();
}

DBusDecodeResult decodeCandidateArgument(const QDBusArgument &argument,
                                         Candidate &destination)
{
    if (!hasSignature(argument, "(usta(sbbsiiduus))")) {
        return {.accepted = false, .reasonCode = QStringLiteral("invalid-dbus-signature")};
    }
    Candidate decoded;
    argument >> decoded;
    const ValidationResult validation = validateCandidate(decoded);
    if (!validation.accepted) {
        return {.accepted = false, .reasonCode = validation.reasonCode};
    }
    destination = std::move(decoded);
    return {.accepted = true, .reasonCode = {}};
}

DBusDecodeResult decodeSnapshotArgument(const QDBusArgument &argument, Snapshot &destination)
{
    if (!hasSignature(
            argument,
            "(ustaya(ssssssiibbbbbsiiiiduusa(siiub))a(suustttu))")) {
        return {.accepted = false, .reasonCode = QStringLiteral("invalid-dbus-signature")};
    }
    Snapshot decoded;
    argument >> decoded;
    const ValidationResult validation = validateSnapshot(decoded);
    if (!validation.accepted) {
        return {.accepted = false, .reasonCode = validation.reasonCode};
    }
    destination = std::move(decoded);
    return {.accepted = true, .reasonCode = {}};
}

DBusDecodeResult decodeOperationResultArgument(const QDBusArgument &argument,
                                               OperationResult &destination)
{
    if (!hasSignature(argument, "(uuusttss)")) {
        return {.accepted = false, .reasonCode = QStringLiteral("invalid-dbus-signature")};
    }
    OperationResult decoded;
    argument >> decoded;
    const ValidationResult validation = validateOperationResult(decoded);
    if (!validation.accepted) {
        return {.accepted = false, .reasonCode = validation.reasonCode};
    }
    destination = std::move(decoded);
    return {.accepted = true, .reasonCode = {}};
}

QDBusArgument &operator<<(QDBusArgument &argument, const Mode &value)
{
    argument.beginStructure();
    argument << value.id << value.pixelSize.width() << value.pixelSize.height()
             << value.refreshMilliHertz << value.preferred;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Mode &value)
{
    int width = 0;
    int height = 0;
    argument.beginStructure();
    argument >> value.id >> width >> height >> value.refreshMilliHertz >> value.preferred;
    argument.endStructure();
    value.pixelSize = QSize(width, height);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Output &value)
{
    argument.beginStructure();
    argument << value.stableId << value.connectorName << value.runtimeCompositorUuid
             << value.label << value.manufacturer << value.model
             << value.physicalSizeMillimeters.width()
             << value.physicalSizeMillimeters.height() << value.hasSerial << value.internal
             << value.ambiguousIdentity << value.enabled << value.primary << value.modeId
             << value.position.x() << value.position.y() << value.logicalSize.width()
             << value.logicalSize.height() << value.scale
             << static_cast<quint32>(value.transform) << value.priority
             << value.replicationSourceStableId;
    writeArray(argument, value.modes);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Output &value)
{
    int physicalWidth = 0;
    int physicalHeight = 0;
    int x = 0;
    int y = 0;
    int logicalWidth = 0;
    int logicalHeight = 0;
    quint32 transform = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.stableId >> value.connectorName >> value.runtimeCompositorUuid
        >> value.label >> value.manufacturer >> value.model >> physicalWidth >> physicalHeight
        >> value.hasSerial >> value.internal >> value.ambiguousIdentity >> value.enabled
        >> value.primary >> value.modeId >> x >> y >> logicalWidth >> logicalHeight
        >> value.scale >> transform >> value.priority >> value.replicationSourceStableId;
    readBoundedArray(argument, value.modes, kMaxModesPerOutput, value.wireValid);
    argument.endStructure();
    value.physicalSizeMillimeters = QSize(physicalWidth, physicalHeight);
    value.position = QPoint(x, y);
    value.logicalSize = QSize(logicalWidth, logicalHeight);
    value.transform = static_cast<Transform>(transform);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const CandidateOutput &value)
{
    argument.beginStructure();
    argument << value.stableId << value.enabled << value.primary << value.modeId
             << value.position.x() << value.position.y() << value.scale
             << static_cast<quint32>(value.transform) << value.priority
             << value.replicationSourceStableId;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, CandidateOutput &value)
{
    int x = 0;
    int y = 0;
    quint32 transform = 0;
    argument.beginStructure();
    argument >> value.stableId >> value.enabled >> value.primary >> value.modeId >> x >> y
        >> value.scale >> transform >> value.priority >> value.replicationSourceStableId;
    argument.endStructure();
    value.position = QPoint(x, y);
    value.transform = static_cast<Transform>(transform);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Candidate &value)
{
    argument.beginStructure();
    argument << value.protocolVersion << value.baseEpoch << value.baseRevision;
    writeArray(argument, value.outputs);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Candidate &value)
{
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.protocolVersion >> value.baseEpoch >> value.baseRevision;
    readBoundedArray(argument, value.outputs, kMaxCandidateOutputs, value.wireValid);
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const TransactionSummary &value)
{
    argument.beginStructure();
    argument << value.transactionId << static_cast<quint32>(value.state)
             << static_cast<quint32>(value.reason) << value.initiatingEpoch
             << value.baseRevision << value.observedRevision
             << value.deadlineMonotonicMilliseconds << value.revertAttempt;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, TransactionSummary &value)
{
    quint32 state = 0;
    quint32 reason = 0;
    argument.beginStructure();
    argument >> value.transactionId >> state >> reason >> value.initiatingEpoch
        >> value.baseRevision >> value.observedRevision
        >> value.deadlineMonotonicMilliseconds >> value.revertAttempt;
    argument.endStructure();
    value.state = static_cast<TransactionState>(state);
    value.reason = static_cast<TransactionReason>(reason);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Snapshot &value)
{
    argument.beginStructure();
    argument << value.protocolVersion << value.serviceEpoch << value.revision
             << value.liveFingerprint;
    writeArray(argument, value.outputs);
    writeArray(argument, value.transactions);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Snapshot &value)
{
    value.wireValid = true;
    argument.beginStructure();
    argument >> value.protocolVersion >> value.serviceEpoch >> value.revision
        >> value.liveFingerprint;
    readBoundedArray(argument, value.outputs, kMaxOutputs, value.wireValid);
    readBoundedArray(argument, value.transactions, kMaxTransactions, value.wireValid);
    argument.endStructure();
    for (const Output &output : value.outputs) {
        value.wireValid = value.wireValid && output.wireValid;
    }
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const OperationResult &value)
{
    argument.beginStructure();
    argument << static_cast<quint32>(value.kind) << static_cast<quint32>(value.status)
             << static_cast<quint32>(value.error) << value.initiatingEpoch
             << value.initiatingRevision << value.observedRevision << value.transactionId
             << value.diagnostic;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, OperationResult &value)
{
    quint32 kind = 0;
    quint32 status = 0;
    quint32 error = 0;
    value.wireValid = true;
    argument.beginStructure();
    argument >> kind >> status >> error >> value.initiatingEpoch >> value.initiatingRevision
        >> value.observedRevision >> value.transactionId >> value.diagnostic;
    argument.endStructure();
    value.kind = static_cast<OperationKind>(kind);
    value.status = static_cast<OperationStatus>(status);
    value.error = static_cast<ErrorCode>(error);
    return argument;
}

} // namespace QindaQt::Display
