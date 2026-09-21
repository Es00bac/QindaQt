// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <algorithm>

namespace QindaQt::Services::Voice {
namespace {

ValidationResult reject(const char *reason)
{
    return {.accepted = false, .reasonCode = QString::fromLatin1(reason)};
}

ValidationResult accept()
{
    return {.accepted = true, .reasonCode = QStringLiteral("ok")};
}

bool withinUtf8Bound(const QString &value, const qsizetype maxUtf8Bytes)
{
    return value.toUtf8().size() <= maxUtf8Bytes;
}

bool hasForbiddenControl(const QString &value, const bool allowLayoutBreaks)
{
    return std::any_of(value.cbegin(), value.cend(), [allowLayoutBreaks](const QChar ch) {
        if (allowLayoutBreaks && (ch == u'\n' || ch == u'\t')) {
            return false;
        }
        // Covers C0, DEL and C1; QChar::isPrint is false for all of them.
        return ch.category() == QChar::Other_Control;
    });
}

bool inRange(const quint32 value, const quint32 maximum)
{
    return value <= maximum;
}

} // namespace

bool isStructuredReasonCode(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    if (utf8.isEmpty() || utf8.size() > kMaxReasonCodeUtf8Bytes) {
        return false;
    }
    return std::all_of(utf8.cbegin(), utf8.cend(), [](const char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') || byte == '-';
    });
}

bool isStructuredIdentifier(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    if (utf8.isEmpty() || utf8.size() > kMaxIdentifierUtf8Bytes) {
        return false;
    }
    return std::all_of(utf8.cbegin(), utf8.cend(), [](const char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') || byte == '-'
               || byte == '_' || byte == '.';
    });
}

bool isDisplayText(const QString &value, const qsizetype maxUtf8Bytes)
{
    return withinUtf8Bound(value, maxUtf8Bytes) && !hasForbiddenControl(value, false);
}

bool isTranscriptText(const QString &value)
{
    return withinUtf8Bound(value, kMaxTranscriptUtf8Bytes)
           && !hasForbiddenControl(value, true);
}

ValidationResult validateSnapshot(const Snapshot &snapshot)
{
    if (!snapshot.wireValid || snapshot.schemaVersion != kSchemaVersion) {
        return reject("unsupported-version");
    }
    if (snapshot.revision == 0) {
        return reject("invalid-revision");
    }
    if (snapshot.state == SessionState::Unknown
        || !inRange(static_cast<quint32>(snapshot.state),
                    static_cast<quint32>(kMaxSessionState))) {
        return reject("unknown-state");
    }
    if (!inRange(static_cast<quint32>(snapshot.mode),
                 static_cast<quint32>(kMaxCaptureMode))) {
        return reject("unknown-mode");
    }
    if (!inRange(static_cast<quint32>(snapshot.lastRoute),
                 static_cast<quint32>(kMaxDeliveryRoute))) {
        return reject("unknown-route");
    }
    if ((snapshot.capabilities & ~kKnownCapabilities) != 0) {
        return reject("unknown-capability");
    }
    if (!isStructuredIdentifier(snapshot.providerId)) {
        return reject("malformed-provider");
    }
    if (!isStructuredIdentifier(snapshot.languageCode)) {
        return reject("malformed-language");
    }
    if (!isDisplayText(snapshot.providerLabel, kMaxLabelUtf8Bytes)
        || !isDisplayText(snapshot.microphoneLabel, kMaxLabelUtf8Bytes)) {
        return reject("malformed-label");
    }
    if (!isDisplayText(snapshot.dictationShortcut, kMaxShortcutUtf8Bytes)
        || !isDisplayText(snapshot.commandShortcut, kMaxShortcutUtf8Bytes)) {
        return reject("malformed-shortcut");
    }
    if (!isTranscriptText(snapshot.partialText) || !isTranscriptText(snapshot.lastText)) {
        return reject("malformed-transcript");
    }
    if (!isStructuredReasonCode(snapshot.reasonCode)) {
        return reject("malformed-reason");
    }
    if (snapshot.providers.size() > kMaxProviders) {
        return reject("too-many-providers");
    }
    bool listsCurrentProvider = snapshot.providers.isEmpty();
    for (const ProviderDescriptor &provider : snapshot.providers) {
        if (!isStructuredIdentifier(provider.id)) {
            return reject("malformed-provider");
        }
        if (!isDisplayText(provider.label, kMaxLabelUtf8Bytes)) {
            return reject("malformed-label");
        }
        listsCurrentProvider = listsCurrentProvider || provider.id == snapshot.providerId;
    }
    // AGENT-GUARD: the settings page selects from this list, so a current
    // provider missing from it would render as "no selection" over a running
    // provider. A provider that advertises a list must include itself.
    if (!listsCurrentProvider) {
        return reject("provider-not-listed");
    }
    // AGENT-GUARD: a partial is live capture state. Leaving one visible after
    // capture ended would show the panel a fragment the user already replaced
    // or discarded, so an idle snapshot carrying one is a provider defect.
    const bool capturing = snapshot.state == SessionState::Arming
                           || snapshot.state == SessionState::Listening
                           || snapshot.state == SessionState::Transcribing;
    if (!capturing && !snapshot.partialText.isEmpty()) {
        return reject("stale-partial");
    }
    if (snapshot.mode == CaptureMode::Command
        && (snapshot.capabilities & CapabilityCommandMode) == 0) {
        return reject("unsupported-mode");
    }
    return accept();
}

ValidationResult validateOperationRequest(const OperationRequest &request)
{
    if (!inRange(static_cast<quint32>(request.kind),
                 static_cast<quint32>(kMaxOperationKind))) {
        return reject("unknown-kind");
    }
    if (request.requestId == 0 || request.expectedRevision == 0) {
        return reject("malformed-request");
    }
    if (request.kind == OperationKind::SetProvider) {
        if (!isStructuredIdentifier(request.providerId)) {
            return reject("malformed-provider");
        }
    } else if (!request.providerId.isEmpty()) {
        return reject("malformed-request");
    }
    return accept();
}

ValidationResult validateOperationResult(const OperationResult &result)
{
    if (!result.wireValid) {
        return reject("malformed-result");
    }
    if (!inRange(static_cast<quint32>(result.kind),
                 static_cast<quint32>(kMaxOperationKind))) {
        return reject("unknown-kind");
    }
    if (!inRange(static_cast<quint32>(result.status),
                 static_cast<quint32>(kMaxOperationStatus))) {
        return reject("unknown-status");
    }
    if (result.requestId == 0 || result.initiatingRevision == 0) {
        return reject("malformed-result");
    }
    // Within one provider ownership revisions only advance. A regression means
    // the reply belongs to a different lineage than the request did.
    if (result.observedRevision < result.initiatingRevision) {
        return reject("revision-regressed");
    }
    if (!isStructuredReasonCode(result.reasonCode)) {
        return reject("malformed-reason");
    }
    return accept();
}

quint32 clampLevelPercent(const quint32 value) noexcept
{
    return std::min(value, kMaxLevelPercent);
}

} // namespace QindaQt::Services::Voice
