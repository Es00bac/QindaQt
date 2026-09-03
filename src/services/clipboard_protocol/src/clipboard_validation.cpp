// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_protocol/clipboard_validation.h>

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>

namespace QindaQt::Services::Clipboard {
namespace {

ValidationResult reject(const char *reason)
{
    return {.accepted = false, .reasonCode = QString::fromLatin1(reason)};
}

bool validKind(const OperationKind kind)
{
    return kind >= OperationKind::Select && kind <= OperationKind::Copy;
}

bool validStatus(const OperationStatus status)
{
    return status >= OperationStatus::Succeeded && status <= OperationStatus::Busy;
}

} // namespace

bool isStructuredReasonCode(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    if (utf8.isEmpty() || utf8.size() > kMaxReasonCodeUtf8Bytes) {
        return false;
    }
    for (const char byte : utf8) {
        if (!((byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9')
              || byte == '-')) {
            return false;
        }
    }
    return true;
}

ValidationResult validateSnapshot(const Snapshot &snapshot)
{
    if (!snapshot.wireValid || snapshot.schemaVersion != kSchemaVersion) {
        return reject("unsupported-version");
    }
    if (snapshot.epoch == 0 || snapshot.generation == 0) {
        return reject("invalid-lineage");
    }
    const auto decoded = ClipboardModel::decodeDescriptorList(snapshot.descriptorList);
    if (!decoded.accepted()) {
        return reject("malformed-descriptors");
    }
    if ((!snapshot.historyEnabled || !snapshot.privacyAllowed)
        && !decoded.descriptors.isEmpty()) {
        return reject("private-content-exposed");
    }
    for (const auto &entry : decoded.descriptors) {
        if (entry.id.generation != snapshot.generation) {
            return reject("entry-generation-mismatch");
        }
    }
    return {.accepted = true, .reasonCode = QStringLiteral("ok")};
}

ValidationResult validateOperationRequest(const OperationRequest &request)
{
    if (!validKind(request.kind) || request.requestId == 0 || request.expectedEpoch == 0
        || request.expectedGeneration == 0) {
        return reject("malformed-request");
    }
    if (request.kind == OperationKind::Clear) {
        if (request.entry.isValid()) {
            return reject("malformed-request");
        }
    } else if (!request.entry.isValid() || request.clearAll) {
        return reject("malformed-request");
    }
    return {.accepted = true, .reasonCode = QStringLiteral("ok")};
}

ValidationResult validateOperationResult(const OperationResult &result)
{
    if (!result.wireValid || !validKind(result.kind) || !validStatus(result.status)
        || result.requestId == 0 || result.initiatingEpoch == 0
        || result.initiatingGeneration == 0 || result.observedEpoch == 0
        || result.observedGeneration == 0 || !isStructuredReasonCode(result.reasonCode)) {
        return reject("malformed-result");
    }
    if (result.status == OperationStatus::Succeeded
        && (result.observedEpoch != result.initiatingEpoch
            || result.observedGeneration != result.initiatingGeneration
            || result.observedRevision < result.initiatingRevision)) {
        return reject("contradictory-success");
    }
    return {.accepted = true, .reasonCode = QStringLiteral("ok")};
}

} // namespace QindaQt::Services::Clipboard
